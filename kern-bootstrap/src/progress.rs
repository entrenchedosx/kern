use std::io::Write;

pub struct Progress {
    pub color: bool,
    pub verbose: bool,
    pub step: usize,
    pub total: usize,
}

impl Progress {
    pub fn new(color: bool, verbose: bool, total: usize) -> Self {
        Self {
            color,
            verbose,
            step: 0,
            total,
        }
    }

    fn paint(&self, s: &str, code: u8) -> String {
        if self.color {
            format!("\x1b[{}m{}\x1b[0m", code, s)
        } else {
            s.to_string()
        }
    }

    pub fn step(&mut self, label: &str) {
        self.step += 1;
        let n = self.step.min(self.total);
        let msg = format!("[{}/{}] {}", n, self.total, label);
        let line = self.paint(&msg, 36); // cyan
        eprintln!("{}", line);
        let _ = std::io::stderr().flush();
    }

    pub fn info(&self, msg: &str) {
        if self.verbose {
            eprintln!("    {}", msg);
        }
    }

    pub fn always(&self, msg: &str) {
        eprintln!("{}", msg);
    }

    pub fn warn(&self, msg: &str) {
        let line = self.paint(msg, 33); // yellow
        eprintln!("{}", line);
    }

    pub fn err(&self, msg: &str) {
        let line = self.paint(msg, 31); // red
        eprintln!("{}", line);
    }

    pub fn ok(&self, msg: &str) {
        let line = self.paint(msg, 32); // green
        eprintln!("{}", line);
    }
}
