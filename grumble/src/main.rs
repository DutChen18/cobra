use clap::Parser;
use std::sync::atomic::{AtomicUsize, Ordering};
use std::sync::Arc;
use tokio::time::Instant;

type Result<T> = std::result::Result<T, Box<dyn std::error::Error + Send + Sync>>;

#[derive(Parser, Debug)]
struct Args {
    #[arg(short, long, default_value_t = 1)]
    jobs: usize,
    #[arg(short, long, default_value_t = usize::MAX)]
    reps: usize,
    #[arg(short, long, default_value_t = usize::MAX)]
    max: usize,
    #[arg(short, long)]
    url: String,
}

struct Context {
    count: AtomicUsize,
    start: Instant,
}

impl Context {
    fn new() -> Context {
        Context {
            count: AtomicUsize::new(0),
            start: Instant::now(),
        }
    }
}

async fn run(context: Arc<Context>, args: Arc<Args>) -> Result<()> {
    let client = reqwest::Client::new();

    for _ in 0..args.reps {
        if context.count.fetch_add(1, Ordering::SeqCst) >= args.max {
            break;
        }

        client.get(&args.url).send().await?.bytes().await?;
    }

    Ok(())
}

#[tokio::main]
async fn main() -> Result<()> {
    let context = Arc::new(Context::new());
    let args = Arc::new(Args::parse());
    let mut handles = Vec::new();

    for _ in 0..args.jobs {
        let context = Arc::clone(&context);
        let args = Arc::clone(&args);

        handles.push(tokio::spawn(run(context, args)));
    }

    for handle in handles {
        handle.await??;
    }

    println!("took {:?}", context.start.elapsed());

    Ok(())
}
