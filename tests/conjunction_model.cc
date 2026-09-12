/// Real-model comparison of scheduled conjunctions with the box constructor.
/// One PNML per invocation; bound externally with timeout 15.
#include <chrono>
#include <iostream>
#include <memory>
#include <numeric>
#include <algorithm>
#include "hsc/core/manager.hh"
#include "hsc/leaves/int_set.hh"
#include "hsc/linear/conjunction/filter.hh"
#include "hsc/linear/equality.hh"
#include "hsc/linear/full.hh"
#include "hsc/petri/parse/PTNetLoader.h"
#include "hsc/petri/invariants.hh"
#include "hsc/petri/nupn.hh"
#include "hsc/petri/decompose.hh"
#include "hsc/order/bandwidth.hh"

int main(int argc, char** argv) {
  if (argc != 2) { std::cerr << "usage: conjunction_model MODEL.pnml\n"; return 2; }
  std::unique_ptr<SparsePetriNet<int>> net(loadXML<int>(argv[1]));
  const auto facts = hsc::petri::pflows_with_inequalities(*net, 2);
  const auto tags = hsc::petri::read_units(argv[1]);
  const auto order = hsc::order::sloan(static_cast<int>(net->getPlaceCount()), hsc::petri::dependency_edges(*net));
  std::vector<std::size_t> pos(order.size());
  for (std::size_t i=0; i<order.size(); ++i) pos[order[i]]=i;
  std::vector<long long> bounds(order.size(), tags.safe ? 1 : -1);
  for (const auto& f : facts.equalities) {
    if (f.constant < 0 || std::any_of(f.terms.begin(), f.terms.end(), [](auto t){ return t.second<=0; })) continue;
    for (auto [p,c] : f.terms) {
      auto& b=bounds[pos[p]];
      b=b<0 ? f.constant/c : std::min(b, f.constant/c);
    }
  }
  for (const auto& f : facts.decreasing) for (auto [p,c] : f.terms) {
    auto& b=bounds[pos[p]];
    b=b<0 ? f.constant/c : std::min(b, f.constant/c);
  }
  if (std::any_of(bounds.begin(),bounds.end(),[](auto b){return b<0 || b>100000;})) {
    std::cerr << "model requires projection or wider domains; skipped\n"; return 2;
  }
  std::vector<hsc::linear::constraint> cs;
  const auto append=[&](const auto& fs, bool inequality, int sign) {
    for (const auto& f:fs) {
      hsc::linear::constraint c; c.target=sign*f.constant; c.at_most=inequality;
      for(auto [p,a]:f.terms) c.terms.emplace_back(pos[p],sign*static_cast<long long>(a));
      cs.push_back(std::move(c));
    }
  };
  append(facts.equalities,false,1); append(facts.decreasing,true,1); append(facts.increasing,true,-1);
  for (bool balanced : {false,true}) {
    hsc::core::manager mgr;
    auto [id,leaf]=mgr.import<hsc::leaves::int_set_theory>();
    const auto ls=mgr.shapes().leaf(id);
    const auto shape=[&](auto&& self,std::size_t lo,std::size_t hi)->hsc::core::shape_code {
      if(lo==hi) return mgr.shapes().unit();
      if(balanced && hi-lo==1) return ls;
      const auto mid=balanced ? lo+(hi-lo)/2 : lo+1;
      return mgr.shapes().pair(balanced ? self(self,lo,mid) : ls,self(self,mid,hi));
    };
    const auto top=shape(shape,0,order.size());
    const auto box=hsc::linear::full_box(mgr,top,[&](std::size_t p){return leaf.interval(0,bounds[p]+1);});
    std::vector<std::vector<std::int32_t>> domains(bounds.size());
    for(std::size_t p=0;p<bounds.size();++p) for(int v=0;v<=bounds[p];++v) domains[p].push_back(v);
    hsc::linear::leaf_access access;
    access.values=[&](std::size_t p)->std::span<const std::int32_t>{return domains[p];};
    access.subset=[&](std::size_t,std::span<const std::int32_t> vs){return leaf.of(vs);};
    auto reference=box;
    for(const auto& c:cs) {
      std::vector<long long> coeff(bounds.size());
      for(auto [p,a]:c.terms) coeff[p]+=a;
      reference=mgr.diagrams().meet(reference,hsc::linear::equality(mgr,top,coeff,c.target,access,c.at_most));
    }
    mgr.set_deadline(std::chrono::steady_clock::now()+std::chrono::seconds(3));
    const auto result=hsc::linear::conjunction(mgr,leaf,top,box,cs);
    if(result!=reference) {std::cerr<<"different diagrams\n";return 1;}
    std::reverse(cs.begin(),cs.end());
    if(hsc::linear::conjunction(mgr,leaf,top,result,cs)!=reference) {std::cerr<<"restricted input mismatch\n";return 1;}
    mgr.set_deadline(std::chrono::steady_clock::now()-std::chrono::seconds(1));
    bool stopped=false;
    try { (void)hsc::linear::conjunction(mgr,leaf,top,box,cs); }
    catch(const hsc::interrupted&) { stopped=true; }
    if(!stopped) {std::cerr<<"deadline ignored\n";return 1;}
    std::cout<<(balanced?"balanced":"spine")<<": identical diagram, restricted input and reordered constraints agree, expired deadline throws\n";
  }
}
