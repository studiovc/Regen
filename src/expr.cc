#include "expr.h"

namespace regen {

const char*
Expr::TypeString(Expr::Type type)
{
  static const char* const type_strings[] = {
    "Literal", "CharClass", "Dot", "BegLine", "EndLine",
    "EOP", "Concat", "Union", "Qmark", "Star", "Plus",
    "Epsilon", "None"
  };

  return type_strings[type];
}

const char*
Expr::SuperTypeString(Expr::SuperType stype)
{
  static const char* const stype_strings[] = {
    "StateExpr", "BinaryExpr", "UnaryExpr"
  };

  return stype_strings[stype];
}

void Expr::Connect(std::set<StateExpr*> &src, std::set<StateExpr*> &dst, bool reverse)
{
  if (reverse) {
    std::set<StateExpr*>::iterator iter = dst.begin();
    while (iter != dst.end()) {
      (*iter)->transition().follow.insert(src.begin(), src.end());
      ++iter;
    }
  } else {
    std::set<StateExpr*>::iterator iter = src.begin();
    while (iter != src.end()) {
      (*iter)->transition().follow.insert(dst.begin(), dst.end());
      ++iter;
    }
  }
}

CharClass::CharClass(StateExpr *e1, StateExpr *e2):
    negative_(false)
{
  Expr *e = e1;
top:
  switch (e->type()) {
    case Expr::kLiteral:
      table_.set(((Literal*)e)->literal());
      break;
    case Expr::kCharClass:
      for (std::size_t i = 0; i < 256; i++) {
        if (((CharClass*)e)->Involve(i)) {
          table_.set(i);
        }
      }
      break;
    case Expr::kDot:
      table_.set();
      return;
    case Expr::kBegLine: case Expr::kEndLine:
      table_.set('\n');
      break;
    default: exitmsg("Invalid Expr Type: %d", e->type());
  }
  if (e == e1) {
    e = e2;
    goto top;
  }

  if (count() >= 128 && !negative()) {
    flip();
    set_negative(true);
  }
}

void StateExpr::TransmitNonGreedy()
{
  if (non_greedy_) {
    std::set<StateExpr*>::iterator iter = transition_.follow.begin();
    while (iter != transition_.follow.end()) {
      if (!((*iter)->non_greedy())) {
        StateExpr* np = (*iter)->non_greedy_pair();
        if (np == NULL) {
          np = static_cast<StateExpr*>((*iter)->Clone());
          (*iter)->set_non_greedy_pair(np);
          np->set_non_greedy();
          np->transition().follow = (*iter)->transition().follow;
          np->TransmitNonGreedy();
        }
        transition_.follow.insert(np);
        transition_.follow.erase(iter++);
        continue;
      }
      iter++;
    }
  }
}

Concat::Concat(Expr *lhs, Expr *rhs):
    BinaryExpr(lhs, rhs)
{
  if (lhs->max_length() == std::numeric_limits<size_t>::max() ||
      rhs->max_length() == std::numeric_limits<size_t>::max()) {
    max_length_ = std::numeric_limits<size_t>::max();
  } else {
    max_length_ = lhs->max_length() + rhs->max_length();
  }
  min_length_ = lhs->min_length() + rhs->min_length();
  nullable_ = lhs->nullable() && rhs->nullable();

  transition_.first = lhs->transition().first;

  if (lhs->nullable()) {
    transition_.first.insert(rhs->transition().first.begin(),
                             rhs->transition().first.end());
  }

  transition_.last = rhs->transition().last;

  if (rhs->nullable()) {
    transition_.last.insert(lhs->transition().last.begin(),
                            lhs->transition().last.end());
  }
}

void Concat::FillTransition(bool reverse)
{
  Connect(lhs_->transition().last, rhs_->transition().first, reverse);
  rhs_->FillTransition(reverse);
  lhs_->FillTransition(reverse);
}

Union::Union(Expr *lhs, Expr *rhs):
    BinaryExpr(lhs, rhs)
{
  max_length_ = std::max(lhs->max_length(), rhs->max_length());
  min_length_ = std::min(lhs->min_length(), rhs->min_length());
  nullable_ = lhs->nullable() || rhs->nullable();

  transition_.first = lhs->transition().first;
  transition_.first.insert(rhs->transition().first.begin(),
                           rhs->transition().first.end());

  transition_.last = lhs->transition().last;
  transition_.last.insert(rhs->transition().last.begin(),
                          rhs->transition().last.end());
}

void Union::FillTransition(bool reverse)
{
  rhs_->FillTransition(reverse);
  lhs_->FillTransition(reverse);
}

Intersection::Intersection(Expr *lhs, Expr *rhs):
    BinaryExpr(lhs, rhs)
{
  max_length_ = std::min(lhs->max_length(), rhs->max_length());
  min_length_ = std::max(lhs->min_length(), rhs->min_length());
  nullable_ = lhs->nullable() && rhs->nullable();

  Operator::NewPair(&op1_, &op2_, Operator::kIntersection);
  lhs_ = new Concat(lhs_, op1_);
  rhs_ = new Concat(rhs_, op2_);

  transition_.first = lhs_->transition().first;
  transition_.first.insert(rhs_->transition().first.begin(),
                           rhs_->transition().first.end());

  transition_.last = lhs_->transition().last;
  transition_.last.insert(rhs_->transition().last.begin(),
                          rhs_->transition().last.end());
}

void Intersection::FillTransition(bool reverse)
{
  rhs_->FillTransition(reverse);
  lhs_->FillTransition(reverse);
}

XOR::XOR(Expr *lhs, Expr *rhs):
    BinaryExpr(lhs, rhs)
{
  max_length_ = std::numeric_limits<size_t>::max();
  min_length_ = std::min(lhs->min_length(), rhs->min_length());
  nullable_ = lhs->nullable() || rhs->nullable();

  Operator::NewPair(&op1_, &op2_, Operator::kXOR);
  lhs_ = new Concat(lhs_, op1_);
  rhs_ = new Concat(rhs_, op2_);

  transition_.first = lhs_->transition().first;
  transition_.first.insert(rhs_->transition().first.begin(),
                           rhs_->transition().first.end());

  transition_.last = lhs_->transition().last;
  transition_.last.insert(rhs_->transition().last.begin(),
                          rhs_->transition().last.end());
}

void XOR::FillTransition(bool reverse)
{
  rhs_->FillTransition(reverse);
  lhs_->FillTransition(reverse);
}

Qmark::Qmark(Expr* lhs, bool non_greedy):
    UnaryExpr(lhs)
{
  max_length_ = lhs->min_length();
  min_length_ = 0;
  nullable_ = true;
  transition_.first = lhs->transition().first;
  transition_.last = lhs->transition().last;
  if (non_greedy) NonGreedify();
}

void Qmark::FillTransition(bool reverse)
{
  lhs_->FillTransition(reverse);
}

Plus::Plus(Expr* lhs):
    UnaryExpr(lhs)
{
  max_length_ = std::numeric_limits<size_t>::max();
  min_length_ = lhs->min_length();
  nullable_ = lhs->nullable();
  transition_.first = lhs->transition().first;
  transition_.last = lhs->transition().last;
}

void Plus::FillTransition(bool reverse)
{
  Connect(lhs_->transition().last, lhs_->transition().first, reverse);
  lhs_->FillTransition(reverse);
}

Star::Star(Expr* lhs, bool non_greedy):
    UnaryExpr(lhs)
{
  max_length_ = std::numeric_limits<size_t>::max();
  min_length_ = 0;
  nullable_ = true;
  transition_.first = lhs->transition().first;
  transition_.last = lhs->transition().last;
  if (non_greedy) NonGreedify();
}

void Star::FillTransition(bool reverse)
{
  Connect(lhs_->transition().last, lhs_->transition().first, reverse);
  lhs_->FillTransition(reverse);
}

Complement::Complement(Expr* lhs, bool loop):
    UnaryExpr(lhs),
    loop_(loop),
    master_(NULL), slave_(NULL)
{
  max_length_ = std::numeric_limits<size_t>::max();
  min_length_ = lhs_->min_length() == 0 ? std::numeric_limits<size_t>::max() : 0;
  nullable_ = !lhs->nullable();
  Operator::NewPair(&master_, &slave_, Operator::kXOR);
  lhs_ = new Concat(lhs_, master_);
  lhs_->FillExpr(slave_);
  if (loop) {
    lhs_ = new Union(new Concat(new Star(new Dot()), slave_), lhs_);
  }
  transition_.first = lhs_->transition().first;
  transition_.last = lhs_->transition().last;
}

void Complement::FillTransition(bool reverse)
{
  lhs_->FillTransition(reverse);
  slave_->transition().follow = master_->transition().follow;
}

} // namespace regen
