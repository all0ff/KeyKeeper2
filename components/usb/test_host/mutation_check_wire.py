#!/usr/bin/env python3
"""Mutation check of the wire path: every mutation of plan_wire / wire_executor / dongle_sink must make
test_wire fail. Usage: python3 mutation_check_wire.py   (run from test_host, needs g++)."""
import os, shutil, subprocess, sys, tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
USB = os.path.join(HERE, "..")
KK = os.path.join(USB, "..", "kkproto")
SRC = ["typing_plan", "typing_runner", "plan_wire", "wire_executor", "dongle_sink", "keycode_map", "cyrillic_layout"]

MUTATIONS = [
    ("plan_wire", "out->chars_after.back() = cum;", "(void)cum;", "skip credit lost"),
    ("plan_wire", "out->chars_before = cum;", "out->chars_before = 0;", "leading skip credit lost"),
    ("plan_wire", "push_wait(out, h.pre_ms - pre_in_event, cum);", "", "long pre wait dropped"),
    ("plan_wire", "push_wait(out, h.gap_ms - gap_in_event, cum);", "", "long gap dropped"),
    ("plan_wire", "if (h.kind == HidEvent::Kind::Key) {\n                    cum += h.chars;", "if (false) {\n                    cum += h.chars;", "key chars not credited"),
    ("wire_executor", "if (e.kind == EventKind::LayoutOff && run_open_) {", "if (false) {", "no closing hotkey after failure"),
    ("wire_executor", "run_open_ = true;\n                    open_event_ = e;", "open_event_ = e;", "run_open not remembered"),
    ("wire_executor", "out.done_events = i + 1;", "out.done_events = i;", "done_events off by one"),
    ("wire_executor", "if (!io.ready()) {\n                    out.code = ResultCode::UsbNotReady;", "if (false) {\n                    out.code = ResultCode::UsbNotReady;", "ready() ignored"),
    ("dongle_sink", "send_batch(channel_, &wire.events[i], 1, &ignored, nullptr);", "(void)ignored;", "no closing batch"),
    ("dongle_sink", "if (n > 0 && ms + em > kMaxBatchMs) {", "if (false) {", "time cap ignored"),
    ("dongle_sink", "if (wire.events[base + i].kind == EventKind::LayoutOff && run_open) {\n                    run_open = false;", "if (false) {\n                    run_open = false;", "same-batch close not tracked"),
    ("dongle_sink", "res.chars_sent = wire.chars_done(failed ? done_total : total);", "res.chars_sent = wire.chars_done(total);", "chars on failure"),
    ("dongle_sink", "run_open = true;\n            } else if (k == EventKind::LayoutOff) {", "} else if (k == EventKind::LayoutOff) {", "run_open never set"),
]

def build(d, out):
    cmd = ["g++", "-std=c++17", "-O1", "-Wall", "-Wextra", "-I" + os.path.join(HERE, "shim"), "-I" + os.path.join(USB, "include"),
           "-I" + os.path.join(KK, "include"), "-I" + HERE, os.path.join(HERE, "test_wire.cpp")]
    cmd += [os.path.join(d, n + ".cpp") for n in SRC] + [os.path.join(KK, "src", "messages.cpp"), "-o", out]
    return subprocess.run(cmd, capture_output=True, text=True)

def main():
    tmp = tempfile.mkdtemp()
    try:
        for n in SRC:
            shutil.copy(os.path.join(USB, "src", n + ".cpp"), os.path.join(tmp, n + ".cpp"))
        r = build(tmp, os.path.join(tmp, "base"))
        if r.returncode:
            print(r.stderr); return 2
        if subprocess.run([os.path.join(tmp, "base"), "1500"], capture_output=True).returncode:
            print("baseline test fails"); return 2
        missed = 0
        for name, old, new, label in MUTATIONS:
            path = os.path.join(tmp, name + ".cpp")
            orig = open(os.path.join(USB, "src", name + ".cpp")).read()
            if old not in orig:
                print("MUTATION TEXT NOT FOUND:", label); missed += 1; continue
            open(path, "w").write(orig.replace(old, new, 1))
            r = build(tmp, os.path.join(tmp, "mut"))
            caught = r.returncode != 0 or subprocess.run([os.path.join(tmp, "mut"), "1500"], capture_output=True).returncode != 0
            print(("caught  " if caught else "MISSED  ") + label)
            missed += 0 if caught else 1
            open(path, "w").write(orig)
        print("all caught" if missed == 0 else "%d not caught" % missed)
        return 1 if missed else 0
    finally:
        shutil.rmtree(tmp, ignore_errors=True)

sys.exit(main())
