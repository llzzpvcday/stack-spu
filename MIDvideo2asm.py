#!/usr/bin/env python

import numpy as np
from sys import stderr, argv
import cv2 as cv
from time import sleep

import pretty_midi


def midi_to_seq(midi_path):

    try:
        midi_data = pretty_midi.PrettyMIDI(midi_path)
    except EOFError:
        return []
    seq = []

    for inst in midi_data.instruments:
        for note in inst.notes:
            freq = pretty_midi.note_number_to_hz(note.pitch)
            dur = note.end - note.start
            seq.append({"freq": freq, "dur": dur, "start": note.start})

    seq.sort(key=lambda x: x["start"])
    return seq


if len(argv) < 3:
    print(f"Usage: {argv[0]} <video name> <midi name>", file=stderr)
    exit(0)

seq = midi_to_seq(argv[2])

cap = cv.VideoCapture(argv[1])
if not cap.isOpened():
    print("Cannot open camera")
    exit()
fps = cap.get(cv.CAP_PROP_FPS)

ret, frame = cap.read()
prev = frame
H, W, _ = frame.shape
print(f"push {W}\npush {H}\nsetscreensize")
print(f"push {fps}\nsetfps")
prev = np.zeros(frame.shape)
cnt = 0

seq_iter = 0
while True:
    cnt += 1
    if cnt % fps == 0:
        print(f"Seconds processed: {cnt // fps}", file=stderr)
    ret, frame = cap.read()
    if not ret:
        break

    cur_time = cnt * (1 / fps)
    if seq_iter < len(seq) and cur_time > seq[seq_iter]["start"]:
        print(f"push {seq[seq_iter]['freq']}")
        print(f"push {seq[seq_iter]['dur']}")
        print(f"push {seq[seq_iter]['start']}")
        print("play")
        seq_iter += 1

    if frame.shape != prev.shape:
        print("Prev and cur shape mismatch", file=stderr)
        break

    frame = frame.astype(np.uint32)
    dif = np.where((frame - prev) != 0)
    # dif = np.where((frame - prev) * (frame - prev) > 100)
    for h, w in zip(dif[0], dif[1]):
        b, g, r = frame[h, w]
        color = r + g * 256 + b * 256 * 256
        print(f"push {color}")
        print(f"pop [{h * W + w}]")
    print("draw")
    prev = frame
