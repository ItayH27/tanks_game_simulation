# Testing

---

The project provides an extensive Google Test suite to verify the accuracy and stability of the game logic.



# What We Test

- **Command-line parsing**
  - Correct recognition of `-comparative` / `-competition`
  - Required arguments, optional `num_threads`, and `-verbose`

- **Error handling**
  - Missing arguments
  - Duplicate keys
  - Unsupported tokens
  - Invalid paths and `num_threads` values

- **Comparative simulator internals**
  - Result comparison
  - Grouping identical outcomes
  - Rendering final boards

- **Comparative simulator outputs**
  - Correct creation of `comparative_results_<time>.txt`
  - Sorted groups and proper output results

- **Competitive simulator internals**
  - Discovering algorithms
  - Loading maps
  - Scheduling games correctly for odd/even N
  - Updating scores

- **Competitive simulator outputs**
  - Correct creation of `competition_<time>.txt`
  - Headers and sorted leaderboard

- **Error reporting**
  - Proper messages for failed map/algorithm loads
  - Missing GameManagers
  - Invalid output file paths

## How to Run Tests

For test running:

```bash
cd build
ctest
```

---

[Back to Table of Contents](../README.md)