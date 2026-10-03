// Sliced background work for the endless world (VISION_PLAN 2.8). M0 prep: not used by gameplay yet.
// Native builds may run a JobQueue on a worker thread; the web build (no pthreads on GitHub Pages) calls pump() from
// the main loop with a per-frame budget. Every Job is sliced: step() does a bounded piece and returns true when done.
#pragma once
#include <algorithm>
#include <chrono>
#include <memory>
#include <utility>
#include <vector>

namespace ew {

struct Job {
  int priority = 0;   // higher runs first
  virtual ~Job() = default;
  virtual bool step(double budgetMs) = 0;   // do at most about budgetMs of work; true = finished
};

class JobQueue {
 public:
  void post(std::unique_ptr<Job> j) {
    jobs_.push_back(std::move(j));
    std::stable_sort(jobs_.begin(), jobs_.end(),
                     [](const std::unique_ptr<Job>& a, const std::unique_ptr<Job>& b) { return a->priority > b->priority; });
  }
  // runs jobs until the budget is spent; returns the number of jobs finished
  int pump(double budgetMs) {
    using clk = std::chrono::steady_clock;
    auto t0 = clk::now();
    int finished = 0;
    while (!jobs_.empty()) {
      double used = std::chrono::duration<double, std::milli>(clk::now() - t0).count();
      if (used >= budgetMs) break;
      if (jobs_.front()->step(budgetMs - used)) { jobs_.erase(jobs_.begin()); finished++; }
    }
    return finished;
  }
  size_t size() const { return jobs_.size(); }
  bool empty() const { return jobs_.empty(); }

 private:
  std::vector<std::unique_ptr<Job>> jobs_;
};

}  // namespace ew
