#ifdef REGEN_ENABLE_PARALLEL
#ifndef REGEN_SSFA_H_
#define  REGEN_SSFA_H_
#include "regex.h"
#include "util.h"
#include "expr.h"
#include "dfa.h"

class Regex;

namespace regen {

class SSFA: public DFA {
public:
  SSFA(Expr* expr_root, const std::vector<StateExpr*> &state_exprs, std::size_t thread_num = 2);
  SSFA(const DFA &dfa, std::size_t thread_num = 2);
  std::size_t thread_num() const { return thread_num_; }
  void thread_num(std::size_t thread_num) { thread_num_ = thread_num; }
  typedef std::map<std::size_t, std::set<std::size_t> > SSTransition;
  typedef std::map<std::size_t, int> SSDTransition;
  void Minimize();
  bool FullMatch(const std::string &str) const { return FullMatch((unsigned char*)str.c_str(), (unsigned char *)str.c_str()+str.length()); }
  bool FullMatch(const unsigned char *str, const unsigned char *end) const;
  struct TaskArg {
    const unsigned char *str;
    const unsigned char *end;
    std::size_t task_id;
  };
private:
  void FullMatchTask(TaskArg targ) const;
  mutable std::vector<int> partial_results_;
  std::size_t nfa_size_;
  std::size_t dfa_size_;
  std::set<std::size_t> start_states_;
  std::size_t thread_num_;
  std::vector<bool> fa_accepts_;
  std::vector<SSTransition> sst_;
};

} // namespace regen
#endif // REGEN_SSFA_H_
#endif // REGEN_ENABLE_PARALLEL