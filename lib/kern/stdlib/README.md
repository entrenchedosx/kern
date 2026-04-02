# Kern expanded standard library (`stdlib/`)

This tree is the **scalable** home for additional pure-Kern modules: small files, clear namespaces, and stable import paths.

## Layout

| Area | Path | Role |
|------|------|------|
| Collections | `stdlib/collections/` | Deques, heaps, multisets, bitmaps, ring buffers, ordered maps |
| Text | `stdlib/text/` | Abbreviations, indentation, whitespace, string similarity |
| Data | `stdlib/data/` | CSV rows, `key=value` lines, dict merge helpers |
| Math | `stdlib/math/` | Angles, 2D vectors, remapping, float comparisons, moments |
| Time | `stdlib/time/` | Durations, simple timers |
| Functional | `stdlib/fun/` | `flip`, multi-step `pipe_chain` |
| System | `stdlib/sys/` | argv token parsing, env snapshots |
| Utilities | `stdlib/util/` | Preconditions, simple validation |
| Algorithms | `stdlib/alg/` | Chunking, sliding windows, permutations, combinations, RLE |
| Iteration | `stdlib/iter/` | `enumerate`, `zip_longest`, `repeat` |
| Graphs | `stdlib/graph/` | Adjacency lists, BFS |
| Bytes | `stdlib/bytes/` | XOR, nibbles (complements `lib/kern/hex_utils.kn`) |
| Result | `stdlib/result/` | Extra helpers (imports `lib/kern/result.kn`) |
| Strings | `stdlib/string/` | ASCII character class tests, padding |
| Trees | `stdlib/tree/` | Binary nodes, traversals |
| Random | `stdlib/random/` | Dice, Fisher–Yates shuffle copy |
| Debug | `stdlib/debug/` | Assertions, shallow `show` |
| Networking | `stdlib/net/` | Minimal URL splitting |
| Encoding | `stdlib/encoding/` | ROT13, Base64 |
| FS | `stdlib/fs/` | Tiny glob helper over `listDir` |

## Importing

Use absolute paths from the repo / distribution root, consistent with the rest of `lib/kern/`:

```text
import("lib/kern/stdlib/collections/deque.kn")
```

For a broad bundle in one script, see `lib/kern/stdlib/catalog.kn` (aggregator imports; omit if you only need a few modules).

## Relationship to top-level `lib/kern/*.kn`

Existing top-level modules (`string_utils.kn`, `algo.kn`, `prelude.kn`, etc.) remain the **default** entry points. The `stdlib/` tree adds **optional**, topic-grouped libraries without breaking older imports.
