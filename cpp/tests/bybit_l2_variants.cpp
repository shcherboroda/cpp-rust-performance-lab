#include "llab/bybit_l2_dense_ladder.hpp"
#include "llab/bybit_l2_ordered_map.hpp"
#include <stdexcept>
int main(){using namespace llab::bybit_l2; std::vector<Level>b{{100,2},{99,1}},a{{101,3}};std::vector<Change>d{{Side::Bid,100,1},{Side::Ask,101,0},{Side::Ask,102,4}};DenseLadderBook dense(90,20);OrderedMapBook tree;if(!dense.apply_snapshot(1,b,a)||!tree.apply_snapshot(1,b,a)||!dense.apply_delta(2,d)||!tree.apply_delta(2,d)||dense.best_bid()!=tree.best_bid()||dense.best_ask()!=tree.best_ask())throw std::runtime_error("variant parity failed");}
