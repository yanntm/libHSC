#include "hsc/core/operation.hh"

namespace hsc::core {

namespace {

code product_at(op_table& ops, const shape_table& shapes, shape_code sort,
                std::span<const code> by_leaf, std::size_t& next) {
  switch (shapes.kind(sort)) {
    case shape_kind::unit:
      return op_table::id;  // no frontier: nothing addresses it
    case shape_kind::leaf:
      return next < by_leaf.size() ? by_leaf[next++] : op_table::id;
    case shape_kind::pair: {
      // Head before tail: the frontier is read left to right.
      const code head = product_at(ops, shapes, shapes.head(sort), by_leaf, next);
      const code tail = product_at(ops, shapes, shapes.tail(sort), by_leaf, next);
      return ops.node(head, tail);
    }
  }
  return op_table::id;
}

}  // namespace

code product(op_table& ops, const shape_table& shapes, shape_code sort,
             std::span<const code> by_leaf) {
  std::size_t next = 0;
  return product_at(ops, shapes, sort, by_leaf, next);
}

}  // namespace hsc::core
