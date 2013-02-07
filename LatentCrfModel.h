#ifndef _AUTO_ENCODER_H_
#define _AUTO_ENCODER_H_

#include <iostream>
#include <fstream>
#include <math.h>
#include <time.h>
#include <set>
#include <algorithm>

#include <boost/mpi/environment.hpp>
#include <boost/mpi/communicator.hpp>
#include <boost/mpi/nonblocking.hpp>
#include <boost/thread/thread.hpp>
#include <boost/serialization/map.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/set.hpp>
#include <boost/mpi/collectives.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/exception/all.hpp>
#include <boost/exception/diagnostic_information.hpp> 
#include <boost/exception_ptr.hpp> 

#include "cdec-utils/logval.h"
#include "cdec-utils/semiring.h"
#define HAVE_BOOST_ARCHIVE_TEXT_OARCHIVE_HPP 1
#include "cdec-utils/fast_sparse_vector.h"

#include "anneal/Cpp/simann.hpp"

#include "StringUtils.h"
#include "FstUtils.h"
#include "LbfgsUtils.h"
#include "LogLinearParams.h"
#include "MultinomialParams.h"
#include "ClustersComparer.h"

using namespace fst;
using namespace std;
namespace mpi = boost::mpi;

// implements the model described at doc/LatentCrfModel.tex
class LatentCrfModel {

  LatentCrfModel(const string &textFilename, 
		 const string &outputPrefix, 
		 LearningInfo &learningInfo);
  
  ~LatentCrfModel();

  static LatentCrfModel *instance;

  // optimize the likelihood with block coordinate descent
  void BlockCoordinateDescent();

  // normalize soft counts with identical content to sum to one
  void NormalizeThetaMle(MultinomialParams::ConditionalMultinomialParam &mle, 
			 map<int, double> &mleMarginals);

  // normalize soft counts with identical content to sum to one
  void NormalizeThetaMle(MultinomialParams::DoubleConditionalMultinomialParam &mle, 
			 map<std::tuple<int, int>, double> &mleMarginals);

  // make sure all lambda features which may fire on this training data are added to lambda.params
  void WarmUp();

  // call back function for simulated annealing
  static float EvaluateNLogLikelihood(float *lambdasArray);

  // lbfgs call back function to compute the negative loglikelihood and its derivatives with respect to lambdas
  static double EvaluateNLogLikelihoodDerivativeWRTLambda(void *ptrFromSentId,
							  const double *lambdasArray,
							  double *gradient,
							  const int lambdasCount,
							  const double step);
  
  // lbfgs call back functiont to report optimizaton progress 
  static int LbfgsProgressReport(void *instance,
				 const lbfgsfloatval_t *x, 
				 const lbfgsfloatval_t *g,
				 const lbfgsfloatval_t fx,
				 const lbfgsfloatval_t xnorm,
				 const lbfgsfloatval_t gnorm,
				 const lbfgsfloatval_t step,
				 int n,
				 int k,
				 int ls);

  // adds up the values in v1 and v2 and returns the summation vector
  static FastSparseVector<double> AccumulateDerivatives(const FastSparseVector<double> &v1, const FastSparseVector<double> &v2);

  // builds an FST to computes B(x,z)
  void BuildThetaLambdaFst(const vector<int> &x, const vector<int> &z, 
			   VectorFst<LogArc> &fst, vector<fst::LogWeight>& alphas, vector<fst::LogWeight>& betas);

  // build an FST to compute Z(x)
  void BuildLambdaFst(const vector<int> &x, VectorFst<LogArc> &fst);

  // build an FST to compute Z(x). also computes potentials
  void BuildLambdaFst(const vector<int> &x, VectorFst<LogArc> &fst, vector<fst::LogWeight> &alphas, vector<fst::LogWeight> &betas);

  // compute the partition function Z_\lambda(x)
  double ComputeNLogZ_lambda(const vector<int> &x); // much slower
  double ComputeNLogZ_lambda(const VectorFst<LogArc> &fst, const vector<fst::LogWeight> &betas); // much faster

  // compute B(x,z) which can be indexed as: BXZ[y^*][z^*] to give B(x, z, z^*, y^*)
  // assumptions: BXZ is cleared
  void ComputeB(const vector<int> &x, const vector<int> &z, 
		const VectorFst<LogArc> &fst, 
		const vector<fst::LogWeight> &alphas, const vector<fst::LogWeight> &betas, 
		map< int, map< int, LogVal<double> > > &BXZ);

  // compute B(x,z) which can be indexed as: BXZ[y^*][z^*] to give B(x, z, z^*, y^*)
  // assumptions: BXZ is cleared
  void ComputeB(const vector<int> &x, const vector<int> &z, 
		const VectorFst<LogArc> &fst, 
		const vector<fst::LogWeight> &alphas, const vector<fst::LogWeight> &betas, 
		map< std::tuple<int, int>, map< int, LogVal<double> > > &BXZ);

  // assumptions: 
  // - fst is populated using BuildLambdaFst()
  // - FXZk is cleared
  void ComputeF(const vector<int> &x, 
		const VectorFst<LogArc> &fst,
		const vector<fst::LogWeight> &alphas, const vector<fst::LogWeight> &betas,
		FastSparseVector<LogVal<double> > &FXZk);

  // assumptions: 
  // - fst is populated using BuildThetaLambdaFst()
  // - DXZk is cleared
  void ComputeD(const vector<int> &x, const vector<int> &z,
		const VectorFst<LogArc> &fst,
		const vector<fst::LogWeight> &alphas, const vector<fst::LogWeight> &betas,
		map<string, double> &DXZk);

  // assumptions: 
  // - fst is populated using BuildThetaLambdaFst()
  // - DXZk is cleared
  void ComputeD(const vector<int> &x, const vector<int> &z, 
		const VectorFst<LogArc> &fst,
		const vector<fst::LogWeight> &alphas, const vector<fst::LogWeight> &betas,
		FastSparseVector<LogVal<double> > &DXZk);
    
  // assumptions:
  // - fst, betas are populated using BuildThetaLambdaFst()
  double ComputeNLogC(const VectorFst<LogArc> &fst,
		      const vector<fst::LogWeight> &betas);
    
  // compute p(y, z | x) = \frac{\prod_i \theta_{z_i|y_i} \exp \lambda h(y_i, y_{i-1}, x, i)}{Z_\lambda(x)}
  double ComputeNLogPrYZGivenX(vector<int>& x, vector<int>& y, vector<int>& z);

  // copute p(y | x, z) = \frac  {\prod_i \theta_{z_i|y_i} \exp \lambda h(y_i, y_{i-1}, x, i)} 
  //                             -------------------------------------------
  //                             {\sum_y' \prod_i \theta_{z_i|y'_i} \exp \lambda h(y'_i, y'_{i-1}, x, i)}
  double ComputeNLogPrYGivenXZ(vector<int> &x, vector<int> &y, vector<int> &z);
    
  double ComputeCorpusNloglikelihood();

  // configure lbfgs parameters according to the LearningInfo member of the model
  lbfgs_parameter_t SetLbfgsConfig();

  // broadcasts the essential member variables in LogLinearParam
  void BroadcastLambdas();
    
  // fire features in this sentence
  void FireFeatures(const vector<int> &x,
		    const VectorFst<LogArc> &fst,
		    FastSparseVector<double> &h);

  // add constrained features with hand-crafted weights
  void AddConstrainedFeatures();

 public:

  static LatentCrfModel& GetInstance();

  static LatentCrfModel& GetInstance(const string &textFilename, 
				     const string &outputPrefix, 
				     LearningInfo &learningInfo);

  // aggregates sets for the mpi reduce operation
  static std::set<std::string> AggregateSets(const std::set<std::string> &v1, const std::set<std::string> &v2);
  
  // aggregates vectors for the mpi reduce operation
  static std::vector<double> AggregateVectors(const std::vector<double> &v1, const std::vector<double> &v2) {
    assert(v1.size() == v2.size());
    std::vector<double> vTotal(v1.size());
    for(unsigned i = 0; i < v1.size(); i++) {
      vTotal[i] = v1[i] + v2[i];
    }
    return vTotal;
  }  
  
  // train the model
  void Train();

  // given an observation sequence x (i.e. tokens), find the most likely label sequence y (i.e. labels)
  void Label(vector<int> &tokens, vector<int> &labels);
  void Label(vector<string> &tokens, vector<int> &labels);
  void Label(vector<vector<int> > &tokens, vector<vector<int> > &lables);
  void Label(vector<vector<string> > &tokens, vector<vector<int> > &labels);
  void Label(string &inputFilename, string &outputFilename);

  // analyze
  void Analyze(string &inputFilename, string &outputFilename);

  // evaluate
  double ComputeVariationOfInformation(std::string &labelsFilename, std::string &goldLabelsFilename);
  double ComputeManyToOne(std::string &aLabelsFilename, std::string &bLabelsFilename);

  void UpdateThetaMleForSent(const unsigned sentId, 
			     MultinomialParams::DoubleConditionalMultinomialParam &mle, 
			     map< std::tuple<int, int> , double > &mleMarginals);

  // collect soft counts from this sentence
  void UpdateThetaMleForSent(const unsigned sentId, 
			     MultinomialParams::ConditionalMultinomialParam &mle, 
			     map<int, double> &mleMarginals);

  vector<vector<int> > data;
  LearningInfo learningInfo;
  MultinomialParams::ConditionalMultinomialParam nLogTheta;
  MultinomialParams::DoubleConditionalMultinomialParam nLogTheta2;
  LogLinearParams *lambda;

 private:
  VocabEncoder vocabEncoder;
  int START_OF_SENTENCE_Y_VALUE, END_OF_SENTENCE_Y_VALUE;
  string textFilename, outputPrefix;
  set<int> xDomain, yDomain;
  // vectors specifiying which feature types to use (initialized in the constructor)
  std::vector<bool> enabledFeatureTypes;
  unsigned countOfConstrainedLambdaParameters;
  double REWARD_FOR_CONSTRAINED_FEATURES, PENALTY_FOR_CONSTRAINED_FEATURES;
  GaussianSampler gaussianSampler;
  SimAnneal simulatedAnnealer;
};

#endif
