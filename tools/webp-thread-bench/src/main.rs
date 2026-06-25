use image::GenericImageView;
use std::env;
use std::fs;
use std::io::{Read, Write};
use std::net::TcpStream;
use std::sync::{Arc, Barrier};
use std::thread;
use std::time::{Duration, Instant};

struct Config {
    input: String,
    seconds: f64,
    warmup_seconds: f64,
    thread_counts: Vec<usize>,
}

struct BenchResult {
    threads: usize,
    decodes: u64,
    elapsed: Duration,
}

fn main() {
    if let Err(error) = run() {
        eprintln!("error: {error}");
        std::process::exit(1);
    }
}

fn run() -> Result<(), String> {
    let config = parse_args()?;
    let bytes = Arc::new(load_input(&config.input)?);
    let (width, height, checksum) = decode_once(&bytes)?;

    println!(
        "input_bytes={} image={}x{} checksum={}",
        bytes.len(),
        width,
        height,
        checksum
    );
    println!(
        "duration_s={:.3} warmup_s={:.3}",
        config.seconds, config.warmup_seconds
    );

    if config.warmup_seconds > 0.0 {
        for &threads in &config.thread_counts {
            let _ = run_phase(
                Arc::clone(&bytes),
                threads,
                Duration::from_secs_f64(config.warmup_seconds),
            )?;
        }
    }

    println!("threads,total_decodes,elapsed_s,decodes_per_s,ms_per_decode,relative_to_1");
    let mut baseline = 0.0;
    for &threads in &config.thread_counts {
        let result = run_phase(
            Arc::clone(&bytes),
            threads,
            Duration::from_secs_f64(config.seconds),
        )?;
        let decodes_per_s = result.decodes as f64 / result.elapsed.as_secs_f64();
        if threads == 1 || baseline <= 0.0 {
            baseline = decodes_per_s;
        }
        let ms_per_decode = 1000.0 / decodes_per_s;
        let relative = if baseline > 0.0 {
            decodes_per_s / baseline
        } else {
            0.0
        };
        println!(
            "{},{},{:.3},{:.2},{:.3},{:.2}x",
            result.threads,
            result.decodes,
            result.elapsed.as_secs_f64(),
            decodes_per_s,
            ms_per_decode,
            relative
        );
    }

    Ok(())
}

fn parse_args() -> Result<Config, String> {
    let mut input = None;
    let mut seconds = 3.0;
    let mut warmup_seconds = 0.5;
    let mut explicit_threads = None;
    let mut max_threads = None;

    let mut args = env::args().skip(1);
    while let Some(arg) = args.next() {
        match arg.as_str() {
            "-h" | "--help" => {
                print_usage();
                std::process::exit(0);
            }
            "--seconds" => {
                seconds = parse_f64_arg("--seconds", args.next())?;
            }
            "--warmup" => {
                warmup_seconds = parse_f64_arg("--warmup", args.next())?;
            }
            "--threads" => {
                explicit_threads = Some(parse_thread_list(args.next())?);
            }
            "--max-threads" => {
                max_threads = Some(parse_usize_arg("--max-threads", args.next())?);
            }
            value if value.starts_with("--seconds=") => {
                seconds = value["--seconds=".len()..]
                    .parse()
                    .map_err(|_| format!("invalid --seconds value: {value}"))?;
            }
            value if value.starts_with("--warmup=") => {
                warmup_seconds = value["--warmup=".len()..]
                    .parse()
                    .map_err(|_| format!("invalid --warmup value: {value}"))?;
            }
            value if value.starts_with("--threads=") => {
                explicit_threads = Some(parse_thread_csv(&value["--threads=".len()..])?);
            }
            value if value.starts_with("--max-threads=") => {
                max_threads = Some(
                    value["--max-threads=".len()..]
                        .parse()
                        .map_err(|_| format!("invalid --max-threads value: {value}"))?,
                );
            }
            value if value.starts_with('-') && value != "-" => {
                return Err(format!("unknown option: {value}"));
            }
            value => {
                if input.replace(value.to_string()).is_some() {
                    return Err("only one input may be provided".to_string());
                }
            }
        }
    }

    let input = input.ok_or_else(|| "missing input path, URL, or -".to_string())?;
    if seconds <= 0.0 {
        return Err("--seconds must be positive".to_string());
    }
    if warmup_seconds < 0.0 {
        return Err("--warmup must be non-negative".to_string());
    }

    let thread_counts = if let Some(threads) = explicit_threads {
        threads
    } else {
        let cpu_count = thread::available_parallelism()
            .map(|count| count.get())
            .unwrap_or(4);
        let max = max_threads.unwrap_or((cpu_count * 2).clamp(1, 16));
        (1..=max).collect()
    };

    Ok(Config {
        input,
        seconds,
        warmup_seconds,
        thread_counts,
    })
}

fn print_usage() {
    println!(
        "usage: webp-thread-bench <path|http-url|-> [--threads 1,2,4] [--max-threads N] [--seconds N] [--warmup N]"
    );
}

fn parse_f64_arg(name: &str, value: Option<String>) -> Result<f64, String> {
    value
        .ok_or_else(|| format!("{name} requires a value"))?
        .parse()
        .map_err(|_| format!("invalid {name} value"))
}

fn parse_usize_arg(name: &str, value: Option<String>) -> Result<usize, String> {
    value
        .ok_or_else(|| format!("{name} requires a value"))?
        .parse()
        .map_err(|_| format!("invalid {name} value"))
}

fn parse_thread_list(value: Option<String>) -> Result<Vec<usize>, String> {
    let value = value.ok_or_else(|| "--threads requires a value".to_string())?;
    parse_thread_csv(&value)
}

fn parse_thread_csv(value: &str) -> Result<Vec<usize>, String> {
    let mut counts = Vec::new();
    for part in value.split(',') {
        let count = part
            .trim()
            .parse::<usize>()
            .map_err(|_| format!("invalid thread count: {part}"))?;
        if count == 0 {
            return Err("thread counts must be positive".to_string());
        }
        if !counts.contains(&count) {
            counts.push(count);
        }
    }
    if counts.is_empty() {
        return Err("--threads must include at least one count".to_string());
    }
    Ok(counts)
}

fn load_input(input: &str) -> Result<Vec<u8>, String> {
    if input == "-" {
        let mut bytes = Vec::new();
        std::io::stdin()
            .read_to_end(&mut bytes)
            .map_err(|error| format!("failed to read stdin: {error}"))?;
        return Ok(bytes);
    }
    if input.starts_with("http://") {
        return http_get(input);
    }
    fs::read(input).map_err(|error| format!("failed to read {input}: {error}"))
}

fn decode_once(bytes: &[u8]) -> Result<(u32, u32, u64), String> {
    let image = image::load_from_memory_with_format(bytes, image::ImageFormat::WebP)
        .map_err(|error| format!("webp decode failed: {error}"))?;
    let (width, height) = image.dimensions();
    let pixels = image.to_rgba8();
    let checksum = pixels
        .as_raw()
        .iter()
        .step_by(4096)
        .fold(width as u64 ^ ((height as u64) << 32), |acc, &byte| {
            acc.wrapping_mul(16_777_619) ^ byte as u64
        });
    Ok((width, height, checksum))
}

fn run_phase(
    bytes: Arc<Vec<u8>>,
    threads: usize,
    duration: Duration,
) -> Result<BenchResult, String> {
    let barrier = Arc::new(Barrier::new(threads + 1));
    let mut handles = Vec::with_capacity(threads);

    for _ in 0..threads {
        let thread_bytes = Arc::clone(&bytes);
        let thread_barrier = Arc::clone(&barrier);
        handles.push(thread::spawn(move || -> Result<(u64, u64), String> {
            thread_barrier.wait();
            let deadline = Instant::now() + duration;
            let mut count = 0;
            let mut checksum = 0;
            while Instant::now() < deadline {
                let (_, _, decoded_checksum) = decode_once(&thread_bytes)?;
                checksum ^= decoded_checksum;
                count += 1;
            }
            Ok((count, checksum))
        }));
    }

    barrier.wait();
    let started = Instant::now();
    let mut decodes = 0;
    let mut checksum = 0;
    for handle in handles {
        let (thread_decodes, thread_checksum) = handle
            .join()
            .map_err(|_| "worker thread panicked".to_string())??;
        decodes += thread_decodes;
        checksum ^= thread_checksum;
    }
    if checksum == u64::MAX {
        eprintln!("unreachable checksum guard: {checksum}");
    }

    Ok(BenchResult {
        threads,
        decodes,
        elapsed: started.elapsed(),
    })
}

fn http_get(url: &str) -> Result<Vec<u8>, String> {
    let (host, port, path) = parse_http_url(url)?;
    let mut stream = TcpStream::connect((host.as_str(), port))
        .map_err(|error| format!("failed to connect to {host}:{port}: {error}"))?;
    let request = format!(
        "GET {path} HTTP/1.1\r\nHost: {host}\r\nUser-Agent: webp-thread-bench\r\nAccept: image/webp,*/*\r\nConnection: close\r\n\r\n"
    );
    stream
        .write_all(request.as_bytes())
        .map_err(|error| format!("failed to write HTTP request: {error}"))?;

    let mut response = Vec::new();
    stream
        .read_to_end(&mut response)
        .map_err(|error| format!("failed to read HTTP response: {error}"))?;

    let header_end = response
        .windows(4)
        .position(|window| window == b"\r\n\r\n")
        .ok_or_else(|| "HTTP response did not contain headers".to_string())?;
    let header_bytes = &response[..header_end];
    let body = response[header_end + 4..].to_vec();
    let headers = String::from_utf8_lossy(header_bytes);
    let mut lines = headers.lines();
    let status = lines
        .next()
        .ok_or_else(|| "empty HTTP response".to_string())?;
    if !status.contains(" 200 ") {
        return Err(format!("unexpected HTTP status: {status}"));
    }

    let chunked = headers.lines().any(|line| {
        let lower = line.to_ascii_lowercase();
        lower.starts_with("transfer-encoding:") && lower.contains("chunked")
    });
    if chunked {
        decode_chunked_body(&body)
    } else {
        Ok(body)
    }
}

fn parse_http_url(url: &str) -> Result<(String, u16, String), String> {
    let rest = url
        .strip_prefix("http://")
        .ok_or_else(|| "only http:// URLs are supported".to_string())?;
    let (authority, path) = match rest.split_once('/') {
        Some((authority, path)) => (authority, format!("/{path}")),
        None => (rest, "/".to_string()),
    };
    let (host, port) = match authority.rsplit_once(':') {
        Some((host, port)) => (
            host.to_string(),
            port.parse()
                .map_err(|_| format!("invalid port in URL: {url}"))?,
        ),
        None => (authority.to_string(), 80),
    };
    if host.is_empty() {
        return Err(format!("invalid URL host: {url}"));
    }
    Ok((host, port, path))
}

fn decode_chunked_body(body: &[u8]) -> Result<Vec<u8>, String> {
    let mut cursor = 0;
    let mut output = Vec::new();
    loop {
        let line_end = find_crlf(body, cursor)
            .ok_or_else(|| "malformed chunked response: missing chunk size".to_string())?;
        let size_line = std::str::from_utf8(&body[cursor..line_end])
            .map_err(|_| "malformed chunk size".to_string())?;
        let size_text = size_line.split(';').next().unwrap_or("").trim();
        let size = usize::from_str_radix(size_text, 16)
            .map_err(|_| format!("malformed chunk size: {size_text}"))?;
        cursor = line_end + 2;
        if size == 0 {
            return Ok(output);
        }
        if cursor + size + 2 > body.len() {
            return Err("malformed chunked response: chunk exceeds body".to_string());
        }
        output.extend_from_slice(&body[cursor..cursor + size]);
        cursor += size;
        if body.get(cursor..cursor + 2) != Some(b"\r\n") {
            return Err("malformed chunked response: missing chunk terminator".to_string());
        }
        cursor += 2;
    }
}

fn find_crlf(bytes: &[u8], start: usize) -> Option<usize> {
    bytes
        .get(start..)?
        .windows(2)
        .position(|window| window == b"\r\n")
        .map(|offset| start + offset)
}
