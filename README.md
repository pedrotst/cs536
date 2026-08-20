# CS 536 — Data Communication and Computer Networks (Purdue, 2019–20)

Graduate networking coursework, taken across Fall 2019 and Spring 2020. Roughly 400 KB of C:
processes, sockets, and a real-time streaming daemon.

## What's worth reading

| Lab | What it is |
|---|---|
| [`fall19/labs/lab1`](fall19/labs/lab1) | `simsh-v1.c` — a small shell: process creation, `exec`, and I/O redirection |
| [`fall19/labs/lab4`](fall19/labs/lab4) | `sockserver.c` / `sockclient.c` — a socket server and client governed by an access-control list |
| [`fall19/labs/lab6`](fall19/labs/lab6) | `streamerd.c` / `playaudio.c` — a real-time audio streaming daemon and the client that plays from it |
| [`fall19/labs/lab3`](fall19/labs/lab3) | packet-level protocol analysis, written up against Wireshark captures |

Each lab keeps its successive versions (`v1`, `v2`, …) as the assignment built up, with the
written answers in LaTeX beside the code.

`spring20/` holds a second pass through the material — further labs and assignments.

## Caveats

Coursework, written to deadlines against a spec, and it reads that way. I scored full marks
on the assignments, which says the programs met the spec, not that they are free of bugs or
especially pleasant to read.
