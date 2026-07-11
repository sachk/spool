#!/usr/bin/env python3
"""Shape a TCP path in userspace for deterministic playback testing."""

from __future__ import annotations

import argparse
import asyncio
import contextlib
import json
import signal
import time
from dataclasses import dataclass


@dataclass(frozen=True)
class Endpoint:
    host: str
    port: int


def parse_endpoint(value: str) -> Endpoint:
    host, separator, port_text = value.rpartition(":")
    host = host.removeprefix("[").removesuffix("]")
    if not separator or not host:
        raise argparse.ArgumentTypeError("expected HOST:PORT")
    try:
        port = int(port_text)
    except ValueError as error:
        raise argparse.ArgumentTypeError("port must be an integer") from error
    if not 1 <= port <= 65535:
        raise argparse.ArgumentTypeError("port must be between 1 and 65535")
    return Endpoint(host, port)


class Metrics:
    def __init__(self) -> None:
        self.started_at = time.monotonic()
        self.active_connections = 0
        self.total_connections = 0
        self.upload_bytes = 0
        self.download_bytes = 0

    def snapshot(self, previous: tuple[float, int, int]) -> tuple[dict[str, float | int], tuple[float, int, int]]:
        now = time.monotonic()
        previous_at, previous_upload, previous_download = previous
        elapsed = max(now - previous_at, 1e-9)
        snapshot: dict[str, float | int] = {
            "active_connections": self.active_connections,
            "total_connections": self.total_connections,
            "upload_bytes": self.upload_bytes,
            "download_bytes": self.download_bytes,
            "upload_bps": round((self.upload_bytes - previous_upload) * 8 / elapsed),
            "download_bps": round((self.download_bytes - previous_download) * 8 / elapsed),
            "uptime_seconds": round(now - self.started_at, 3),
        }
        return snapshot, (now, self.upload_bytes, self.download_bytes)


class Pacer:
    def __init__(self, bytes_per_second: float) -> None:
        self._bytes_per_second = bytes_per_second
        self._next_send_at = 0.0

    async def wait(self, byte_count: int) -> None:
        if self._bytes_per_second <= 0:
            return
        loop = asyncio.get_running_loop()
        now = loop.time()
        send_at = max(now, self._next_send_at)
        self._next_send_at = send_at + byte_count / self._bytes_per_second
        if send_at > now:
            await asyncio.sleep(send_at - now)


async def relay(
    reader: asyncio.StreamReader,
    writer: asyncio.StreamWriter,
    *,
    latency_seconds: float,
    pacer: Pacer,
    chunk_bytes: int,
    on_bytes,
) -> None:
    queue: asyncio.Queue[tuple[float, bytes] | None] = asyncio.Queue(maxsize=64)
    loop = asyncio.get_running_loop()

    async def receive() -> None:
        while data := await reader.read(chunk_bytes):
            await queue.put((loop.time() + latency_seconds, data))
        await queue.put(None)

    async def transmit() -> None:
        while queued := await queue.get():
            ready_at, data = queued
            delay = ready_at - loop.time()
            if delay > 0:
                await asyncio.sleep(delay)
            await pacer.wait(len(data))
            writer.write(data)
            await writer.drain()
            on_bytes(len(data))
        if writer.can_write_eof():
            writer.write_eof()
            await writer.drain()

    async with asyncio.TaskGroup() as tasks:
        tasks.create_task(receive())
        tasks.create_task(transmit())


class Shaper:
    def __init__(
        self,
        upstream: Endpoint,
        *,
        latency_seconds: float,
        upload_bytes_per_second: float,
        download_bytes_per_second: float,
        chunk_bytes: int,
    ) -> None:
        self.upstream = upstream
        self.latency_seconds = latency_seconds
        self.upload_pacer = Pacer(upload_bytes_per_second)
        self.download_pacer = Pacer(download_bytes_per_second)
        self.chunk_bytes = chunk_bytes
        self.metrics = Metrics()
        self.connections: set[asyncio.Task[None]] = set()

    async def handle(self, client_reader: asyncio.StreamReader, client_writer: asyncio.StreamWriter) -> None:
        task = asyncio.current_task()
        if task is not None:
            self.connections.add(task)
        self.metrics.active_connections += 1
        self.metrics.total_connections += 1
        try:
            upstream_reader, upstream_writer = await asyncio.open_connection(
                self.upstream.host, self.upstream.port
            )
            try:
                async with asyncio.TaskGroup() as relays:
                    relays.create_task(
                        relay(
                            client_reader,
                            upstream_writer,
                            latency_seconds=self.latency_seconds,
                            pacer=self.upload_pacer,
                            chunk_bytes=self.chunk_bytes,
                            on_bytes=self._record_upload,
                        )
                    )
                    relays.create_task(
                        relay(
                            upstream_reader,
                            client_writer,
                            latency_seconds=self.latency_seconds,
                            pacer=self.download_pacer,
                            chunk_bytes=self.chunk_bytes,
                            on_bytes=self._record_download,
                        )
                    )
            finally:
                upstream_writer.close()
                await upstream_writer.wait_closed()
        except* (ConnectionError, OSError) as errors:
            print(json.dumps({"event": "connection_error", "error": str(errors)}), flush=True)
        finally:
            client_writer.close()
            with contextlib.suppress(ConnectionError, OSError):
                await client_writer.wait_closed()
            self.metrics.active_connections -= 1
            if task is not None:
                self.connections.discard(task)

    def _record_upload(self, byte_count: int) -> None:
        self.metrics.upload_bytes += byte_count

    def _record_download(self, byte_count: int) -> None:
        self.metrics.download_bytes += byte_count


async def report_metrics(metrics: Metrics, interval: float) -> None:
    previous = (time.monotonic(), 0, 0)
    while True:
        await asyncio.sleep(interval)
        snapshot, previous = metrics.snapshot(previous)
        print(json.dumps({"event": "metrics", **snapshot}, sort_keys=True), flush=True)


async def run(args: argparse.Namespace) -> None:
    one_way_latency = args.rtt_ms / 2000
    shaper = Shaper(
        args.upstream,
        latency_seconds=one_way_latency,
        upload_bytes_per_second=args.upload_mbps * 1_000_000 / 8,
        download_bytes_per_second=args.download_mbps * 1_000_000 / 8,
        chunk_bytes=args.chunk_bytes,
    )
    stop = asyncio.Event()
    loop = asyncio.get_running_loop()
    for caught_signal in (signal.SIGINT, signal.SIGTERM):
        loop.add_signal_handler(caught_signal, stop.set)

    server = await asyncio.start_server(shaper.handle, args.listen.host, args.listen.port)
    sockets = server.sockets or []
    addresses = [socket.getsockname()[:2] for socket in sockets]
    print(
        json.dumps(
            {
                "event": "listening",
                "addresses": addresses,
                "upstream": [args.upstream.host, args.upstream.port],
                "rtt_ms": args.rtt_ms,
                "upload_mbps": args.upload_mbps,
                "download_mbps": args.download_mbps,
            },
            sort_keys=True,
        ),
        flush=True,
    )

    reporter = (
        asyncio.create_task(report_metrics(shaper.metrics, args.metrics_interval))
        if args.metrics_interval > 0
        else None
    )
    async with server:
        await stop.wait()

    if reporter is not None:
        reporter.cancel()
        with contextlib.suppress(asyncio.CancelledError):
            await reporter
    for connection in tuple(shaper.connections):
        connection.cancel()
    if shaper.connections:
        await asyncio.gather(*shaper.connections, return_exceptions=True)


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Forward TCP traffic with fixed RTT and directional bandwidth limits."
    )
    parser.add_argument("--listen", type=parse_endpoint, default=parse_endpoint("127.0.0.1:18096"))
    parser.add_argument("--upstream", type=parse_endpoint, required=True)
    parser.add_argument("--rtt-ms", type=float, default=0, help="round-trip latency in milliseconds")
    parser.add_argument("--upload-mbps", type=float, default=0, help="upload limit; zero is unlimited")
    parser.add_argument("--download-mbps", type=float, default=0, help="download limit; zero is unlimited")
    parser.add_argument("--chunk-bytes", type=int, default=16 * 1024)
    parser.add_argument("--metrics-interval", type=float, default=1.0, help="JSON metrics interval; zero disables")
    return parser


def main() -> None:
    parser = build_parser()
    args = parser.parse_args()
    if args.rtt_ms < 0 or args.upload_mbps < 0 or args.download_mbps < 0:
        parser.error("latency and bandwidth limits cannot be negative")
    if not 1024 <= args.chunk_bytes <= 1024 * 1024:
        parser.error("--chunk-bytes must be between 1024 and 1048576")
    if args.metrics_interval < 0:
        parser.error("--metrics-interval cannot be negative")
    asyncio.run(run(args))


if __name__ == "__main__":
    main()
