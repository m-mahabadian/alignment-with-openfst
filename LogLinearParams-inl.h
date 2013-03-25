#ifndef _LOG_LINEAR_PARAMS_INL_H_
#define _LOG_LINEAR_PARAMS_INL_H_

public:
  // given description of an arc in the alignment transducer, compute the local arc probability
  inline double ComputeLogProb(int srcToken, int prevSrcToken, int tgtToken, int srcPos, int prevSrcPos, int tgtPos, 
					 int srcSentLength, int tgtSentLength, 
					 const std::vector<bool>& enabledFeatureTypes) {
    std::map<std::string, double> activeFeatures;
    FireFeatures(srcToken, prevSrcToken, tgtToken, srcPos, prevSrcPos, tgtPos, srcSentLength, tgtSentLength, enabledFeatureTypes, activeFeatures);
    // compute log prob
    double result = DotProduct(activeFeatures);    
    return result;
  }
  
  // update a single parameter's value (adds the parameter if necessary)
  inline void UpdateParam(const std::string &paramId, const double newValue) {
    if(!AddParam(paramId, newValue)) {
      paramWeights[paramIndexes[paramId]] = newValue;
    }
  }

  // update a single parameter's value, identified by its integer index in the weights array
  // assumptions:
  // - there's a parameter with such index
  inline void UpdateParam(const unsigned paramIndex, const double newValue) {
    if(paramWeights.size() <= paramIndex) {
      assert(false);
    }
    paramWeights[paramIndex] = newValue;
  }

  // copies the weight of the specified feature from paramWeights vector to oldParamWeights vector
  inline void UpdateOldParamWeight(const std::string &paramId) {
    if(!AddParam(paramId)) {
      oldParamWeights[paramIndexes[paramId]] = paramWeights[paramIndexes[paramId]];
    }
  }

  // copies the weight of all features from paramWeights vector to oldParamWeights vector
  inline void UpdateOldParamWeights() {
    assert(oldParamWeights.size() == paramWeights.size());
    for(int i = 0; i < paramWeights.size(); i++) {
      oldParamWeights[i] = paramWeights[i];
    }
  }  

  // checks whether a parameter exists
  inline bool ParamExists(const std::string &paramId) {
    return paramIndexes.count(paramId) == 1;
  }

  // checks whether a parameter exists
  inline bool ParamExists(const unsigned &paramIndex) {
    return paramIndex < paramIndexes.size();
  }

  // returns the int index of the parameter in the underlying array
  inline unsigned GetParamIndex(const std::string &paramId) {
    AddParam(paramId);
    return paramIndexes[paramId];
  }

  // returns the string identifier of the parameter given its int index in the weights array
  inline std::string GetParamId(const unsigned paramIndex) {
    assert(paramIndex < paramWeights.size());
    return paramIds[paramIndex];
  }

  // returns the current weight of this param (adds the parameter if necessary)
  inline double GetParamWeight(const std::string &paramId) {
    AddParam(paramId);
    return paramWeights[paramIndexes[paramId]];
  }
  
  // returns the difference between new and old weights of a parameter, given its string ID. 
  // assumptions:
  // - paramId already exists
  inline double GetParamNewMinusOldWeight(const std::string &paramId) {
    return paramWeights[paramIndexes[paramId]] - oldParamWeights[paramIndexes[paramId]];
  }

  // returns the difference between new and old weights of a parameter, given its vector index
  // assumptions:
  // - paramIndex is a valid index
  inline double GetParamNewMinusOldWeight(const unsigned paramIndex) {
    return paramWeights[paramIndex] - oldParamWeights[paramIndex];
  }

  inline int GetParamsCount() {
    assert(paramWeights.size() == paramIndexes.size());
    return paramWeights.size();
  }

  // returns a pointer to the array of parameter weights
   inline double* GetParamWeightsArray() { 
     return paramWeights.data(); 
   } 

   // returns a pointer to the array of old parameter weights 
   inline double* GetOldParamWeightsArray() { 
     return oldParamWeights.data(); 
   } 

   // clear model parameters 
   inline void Clear() { 
     paramIndexes.clear(); 
     paramWeights.clear(); 
   } 

#endif