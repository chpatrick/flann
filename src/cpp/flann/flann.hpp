/***********************************************************************
 * Software License Agreement (BSD License)
 *
 * Copyright 2008-2009  Marius Muja (mariusm@cs.ubc.ca). All rights reserved.
 * Copyright 2008-2009  David G. Lowe (lowe@cs.ubc.ca). All rights reserved.
 *
 * THE BSD LICENSE
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *************************************************************************/

#ifndef FLANN_HPP_
#define FLANN_HPP_

#include <vector>
#include <string>
#include <cassert>
#include <cstdio>

#include "flann/flann.h"
#include "flann/general.h"
#include "flann/util/matrix.h"
#include "flann/util/result_set.h"
#include "flann/nn/index_testing.h"
#include "flann/util/object_factory.h"
#include "flann/util/saving.h"

#include "flann/algorithms/all_indices.h"

namespace flann
{

struct SavedIndexParams : public IndexParams {
	SavedIndexParams(std::string filename_) : IndexParams(SAVED), filename(filename_) {}

	std::string filename;		// filename of the stored index

	flann_algorithm_t getIndexType() const { return algorithm; }

	void fromParameters(const FLANNParameters& p)
	{
		assert(p.algorithm==algorithm);
		filename = p.filename;
	};

	void toParameters(FLANNParameters& p) const
	{
		p.algorithm = algorithm;
		p.filename = filename.c_str();
	};

	void print() const
	{
		printf("Index type: %d\n",(int)algorithm);
		printf("Filename: %s\n", filename.c_str());
	}
};

template<typename T>
class Index {
	NNIndex<T>* nnIndex;

public:
	Index(const Matrix<T>& features, const IndexParams& params);

	~Index();

	void buildIndex();

	void knnSearch(const Matrix<T>& queries, Matrix<int>& indices, Matrix<float>& dists, int knn, const SearchParams& params);

	int radiusSearch(const Matrix<T>& query, Matrix<int>& indices, Matrix<float>& dists, float radius, const SearchParams& params);

	void save(std::string filename);

	int veclen() const;

	int size() const;

	NNIndex<T>* getIndex() { return nnIndex; }

	const IndexParams* getIndexParameters() { return nnIndex->getParameters(); }
};


template<typename T>
NNIndex<T>* load_saved_index(const Matrix<T>& dataset, const string& filename)
{
	FILE* fin = fopen(filename.c_str(), "rb");
	if (fin==NULL) {
		return NULL;
	}
	IndexHeader header = load_header(fin);
	if (header.data_type!=get_flann_datatype<T>()) {
		throw FLANNException("Datatype of saved index is different than of the one to be created.");
	}
	if (size_t(header.rows)!=dataset.rows || size_t(header.cols)!=dataset.cols) {
		throw FLANNException("The index saved belongs to a different dataset");
	}

	IndexParams* params = ParamsFactory::instance().create(header.index_type);
	NNIndex<T>* nnIndex = create_index_by_type(dataset, *params);
	nnIndex->loadIndex(fin);
	fclose(fin);

	return nnIndex;
}


template<typename T>
Index<T>::Index(const Matrix<T>& dataset, const IndexParams& params)
{
	flann_algorithm_t index_type = params.getIndexType();

	if (index_type==SAVED) {
		nnIndex = load_saved_index(dataset, ((const SavedIndexParams&)params).filename);
	}
	else {
		nnIndex = create_index_by_type(dataset, params);
	}
}

template<typename T>
Index<T>::~Index()
{
	delete nnIndex;
}

template<typename T>
void Index<T>::buildIndex()
{
	nnIndex->buildIndex();
}

template<typename T>
void Index<T>::knnSearch(const Matrix<T>& queries, Matrix<int>& indices, Matrix<float>& dists, int knn, const SearchParams& searchParams)
{
	assert(queries.cols==nnIndex->veclen());
	assert(indices.rows>=queries.rows);
	assert(dists.rows>=queries.rows);
	assert(int(indices.cols)>=knn);
	assert(int(dists.cols)>=knn);

    KNNResultSet<T> resultSet(knn);

    for (size_t i = 0; i < queries.rows; i++) {
        T* target = queries[i];
        resultSet.init(target, queries.cols);

        nnIndex->findNeighbors(resultSet, target, searchParams);

        int* neighbors = resultSet.getNeighbors();
        float* distances = resultSet.getDistances();
        memcpy(indices[i], neighbors, knn*sizeof(int));
        memcpy(dists[i], distances, knn*sizeof(float));
    }
}

template<typename T>
int Index<T>::radiusSearch(const Matrix<T>& query, Matrix<int>& indices, Matrix<float>& dists, float radius, const SearchParams& searchParams)
{
	if (query.rows!=1) {
		printf("I can only search one feature at a time for range search\n");
		return -1;
	}
	assert(query.cols==nnIndex->veclen());

	RadiusResultSet<T> resultSet(radius);
	resultSet.init(query.data, query.cols);
	nnIndex->findNeighbors(resultSet,query.data,searchParams);

	// TODO: optimise here
	int* neighbors = resultSet.getNeighbors();
	float* distances = resultSet.getDistances();
	size_t count_nn = min(resultSet.size(), indices.cols);

	assert (dists.cols>=count_nn);

	for (size_t i=0;i<count_nn;++i) {
		indices[0][i] = neighbors[i];
		dists[0][i] = distances[i];
	}

	return count_nn;
}


template<typename T>
void Index<T>::save(string filename)
{
	FILE* fout = fopen(filename.c_str(), "wb");
	if (fout==NULL) {
		throw FLANNException("Cannot open file");
	}
	save_header(fout, *nnIndex);
	nnIndex->saveIndex(fout);
	fclose(fout);
}


template<typename T>
int Index<T>::size() const
{
	return nnIndex->size();
}

template<typename T>
int Index<T>::veclen() const
{
	return nnIndex->veclen();
}


template <typename ELEM_TYPE, typename DIST_TYPE>
int hierarchicalClustering(const Matrix<ELEM_TYPE>& features, Matrix<DIST_TYPE>& centers, const KMeansIndexParams& params)
{
    KMeansIndex<ELEM_TYPE> kmeans(features, params);
	kmeans.buildIndex();

    int clusterNum = kmeans.getClusterCenters(centers);
	return clusterNum;
}

}
#endif /* FLANN_HPP_ */