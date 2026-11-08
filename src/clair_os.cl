// V0.56: the module os (import os), written in Clair and built into the compiler.
// The work is done in C (clair_os_rt.c) through the __os_... functions, which only this file can call.

// What a program gave: its exit code (0: all went well), its output and its errors.
Class RunResult {
    code: int
    out: string
    err: string
}

// A program started with os.start(), still running or not.
Class Process {
    id: int

    // what it wrote since the last read ("" when nothing): never waits
    def read() -> string {
        return __os_read(self.id, 0)
    }

    def read_errors() -> string {
        return __os_read(self.id, 1)
    }

    // gives it text to read (its input)
    def write(text: string) -> bool {
        return __os_write(self.id, text)
    }

    // the end of its input
    def close_input() -> bool {
        return __os_close_input(self.id)
    }

    def running() -> bool {
        return __os_running(self.id)
    }

    // stops it, and what it started
    def stop() -> bool {
        return __os_stop(self.id)
    }

    // waits until it stops, gives its code (128 + the signal when it was stopped)
    def wait() -> int {
        return __os_wait(self.id, -1.0)
    }

    // waits at most seconds: its code, or -1 when it still runs
    def wait_for(seconds: float) -> int {
        return __os_wait(self.id, seconds)
    }
}

// ---- programs ----

// runs a program and waits for it: os.run(["clang", "--version"])
def run(command: list[string]) -> RunResult {
    return run_with(command, "", -1.0)
}

// the same, with text for its input, stopped after seconds (< 0: never)
def run_with(command: list[string], stdin_text: string, seconds: float) -> RunResult {
    let p: Process = Process(__os_start(command, false))
    if p.id == 0 {
        return RunResult(127, "", error() + "\n")
    }
    if stdin_text != "" {
        p.write(stdin_text)
    }
    p.close_input()
    let code: int = __os_wait(p.id, seconds)
    if code == -1 {
        p.stop()
        code = __os_wait(p.id, -1.0)
    }
    let out: string = p.read()
    let err: string = p.read_errors()
    return RunResult(code, out, err)
}

// a command line for the shell: os.shell("ls -l | wc -l")
def shell(command: string) -> RunResult {
    return run(["sh", "-c", command])
}

// starts a program without waiting: read its output while it runs
def start(command: list[string]) -> Process {
    return Process(__os_start(command, false))
}

// the same, its errors mixed with its output, in the order it wrote them
def start_merged(command: list[string]) -> Process {
    return Process(__os_start(command, true))
}

// where a program is ("" when it is not installed)
def which(name: string) -> string {
    return __os_which(name)
}

// ---- the program itself ----

// what follows its name on the command line: ./prog a b gives ["a", "b"]
def args() -> list[string] {
    return __os_args()
}

def env(name: string, otherwise: string) -> string {
    return __os_env(name, otherwise)
}

def set_env(name: string, value: string) -> bool {
    return __os_set_env(name, value)
}

def home() -> string {
    return __os_env("HOME", "/")
}

// stops the program now, with this code
def exit(code: int) -> int {
    return __os_exit(code)
}

// what went wrong last (when a function gave false)
def error() -> string {
    return __os_error()
}

// ---- files and folders ----

// the names in a folder, sorted
def listdir(path: string) -> list[string] {
    return __os_listdir(path)
}

def exists(path: string) -> bool {
    return __os_stat(path, 0) == 1
}

def is_dir(path: string) -> bool {
    return __os_stat(path, 1) == 1
}

def is_file(path: string) -> bool {
    return __os_stat(path, 2) == 1
}

// in bytes, -1 when there is no such file
def size(path: string) -> int {
    return __os_stat(path, 3)
}

// when it was last changed, in seconds since 1970 (-1: no such file)
def modified(path: string) -> int {
    return __os_stat(path, 4)
}

// the folder, and those above it
def mkdir(path: string) -> bool {
    return __os_mkdir(path)
}

// a file, or an empty folder
def remove(path: string) -> bool {
    return __os_remove(path)
}

def rename(path: string, to: string) -> bool {
    return __os_rename(path, to)
}

def copy_file(path: string, to: string) -> bool {
    return __os_copy_file(path, to)
}

def cwd() -> string {
    return __os_cwd()
}

def chdir(path: string) -> bool {
    return __os_chdir(path)
}

// ---- paths ----

// os.join("src", "main.cl") is "src/main.cl"
def join(a: string, b: string) -> string {
    return __os_join(a, b)
}

// "a/b.cl": "b.cl"
def basename(path: string) -> string {
    return __os_path(0, path)
}

// "a/b.cl": "a"
def dirname(path: string) -> string {
    return __os_path(1, path)
}

// "a/b.cl": ".cl"
def extension(path: string) -> string {
    return __os_path(2, path)
}

// ---- time ----

// seconds since 1970 (the clock of the phone)
def time() -> float {
    return __os_time(0)
}

// seconds of a clock that only goes forward: the difference of two gives a duration
def clock() -> float {
    return __os_time(1)
}

def sleep(seconds: float) -> int {
    return __os_sleep(seconds)
}
