#include "llab/bybit_l2_dense_ladder.hpp"
#include "llab/bybit_l2_order_book.hpp"
#include "llab/bybit_l2_ordered_map.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <iostream>
#include <numeric>
#include <vector>
using namespace llab::bybit_l2;
constexpr std::size_t events=100000,warmups=10,samples=200; std::atomic<Quantity> sink{};
std::vector<Level> levels(Side s,std::size_t n,Price gap){std::vector<Level>x;for(std::size_t i=0;i<n;++i)x.push_back({s==Side::Bid?2000000-i*gap:2000001+i*gap,100+i});return x;}
template<class B> double run(B& x,const std::vector<Level>&b,const std::vector<Level>&a,const std::vector<Change>&e){if(!x.apply_snapshot(1,b,a))std::terminate();auto t=std::chrono::steady_clock::now();for(std::size_t i=0;i<e.size();++i)if(!x.apply_delta(i+2,{e[i]}))std::terminate();auto n=std::chrono::duration<double,std::nano>(std::chrono::steady_clock::now()-t).count()/e.size();sink.fetch_xor(x.best_bid()->quantity,std::memory_order_relaxed);return n;}
int main(int argc,char**argv){if(argc!=3)return 2;std::string v=argv[1],w=argv[2];auto n=w=="wide"?1000:50;auto gap=w=="wide"?1000:2;auto b=levels(Side::Bid,n,gap),a=levels(Side::Ask,n,gap);std::vector<Change> e;e.reserve(events);for(std::size_t i=0;i<events;++i){auto side=(i/2)%2?Side::Bid:Side::Ask;auto idx=((i/2)/2)%n;auto base=side==Side::Bid?b[idx].price:a[idx].price;auto round=(i/2)/(2*n);auto active=base+(round%2?gap/2:0),other=base+(round%2?0:gap/2);if(w=="replace")e.push_back({side,base,100+i%97});else if(i%2==0)e.push_back({side,active,0});else e.push_back({side,other,100+i%97});}auto one=[&](){if(v=="vector"){OrderBook x(n);return run(x,b,a,e);}if(v=="dense"){DenseLadderBook x(0,3000000);return run(x,b,a,e);}if(v=="map"){OrderedMapBook x;return run(x,b,a,e);}std::terminate();};for(size_t i=0;i<warmups;++i)one();std::vector<double>s(samples);for(auto&x:s)x=one();auto q=s;std::sort(q.begin(),q.end());std::cout<<"variant="<<v<<" workload="<<w<<" levels="<<n<<" events="<<events<<" samples=200 p50_ns="<<q[99]<<" p95_ns="<<q[189]<<" p99_ns="<<q[197]<<" p995_ns="<<q[198]<<" max_ns="<<q[199]<<" mean_ns="<<std::accumulate(s.begin(),s.end(),0.0)/samples<<'\n';}
