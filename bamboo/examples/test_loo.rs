use bamboo_core::{Engine, InputMethod, Mode};

fn main() {
    let mut e = Engine::new(InputMethod::telex());
    
    // Test char-by-char like skey does
    let tests = vec!["l", "lo", "loo", "lô"];
    for input in &tests {
        let im = e.input_method().clone();
        let cfg = e.config();
        e = Engine::with_config(im, cfg);
        let out = e.process(*input, Mode::Vietnamese);
        println!("process('{}') → '{}'", input, out);
    }
    
    // Test after multiple resets (simulating skey's flow)
    for _ in 0..5 {
        let im = e.input_method().clone();
        let cfg = e.config();
        e = Engine::with_config(im, cfg);
    }
    let out = e.process("loo", Mode::Vietnamese);
    println!("After 5 resets, process('loo') → '{}'", out);
}
