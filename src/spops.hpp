/*
 * spops.cpp: Sparse Matrix operations implementation
 * Sparse Matrix - Sparse Matrix (SpMM)
 * (c) Mohammad Hasanzadeh Mofrad, 2019
 * (e) m.hasanzadeh.mofrad@gmail.com
 */
 
#ifndef SPOPS_H
#define SPOPS_H

#include "env.hpp"
#include "spmat.hpp"

template<typename Weight>
inline std::tuple<uint64_t, uint32_t, uint32_t> spmm_sym(std::shared_ptr<struct Compressed_Format<Weight>> A,
                                                         std::shared_ptr<struct Compressed_Format<Weight>> B,
                                                         std::vector<Weight> s,
                                                         int32_t tid) {

    uint64_t nnzmax = 0;
    uint32_t nrows = 0;
    uint32_t ncols = 0;
    /*
    if((A->compression_type == COMPRESSED_FORMAT::_CSR_) and (B->compression_type == COMPRESSED_FORMAT::_CSR_)) {
        const std::shared_ptr<struct CSR<Weight>> A_CSR = std::static_pointer_cast<struct CSR<Weight>>(A);
        const uint64_t A_nnz   = A_CSR->nnz;
        const uint32_t A_nrows = A_CSR->nrows;
        const uint32_t A_ncols = A_CSR->ncols;
        const uint32_t* A_IA   = A_CSR->IA_blk->ptr;
        const uint32_t* A_JA   = A_CSR->JA_blk->ptr;
        const Weight*    A_A   = A_CSR->A_blk->ptr;
        
        const std::shared_ptr<struct CSR<Weight>> B_CSR = std::static_pointer_cast<struct CSR<Weight>>(B);
        const uint64_t B_nnz   = B_CSR->nnz;
        const uint32_t B_nrows = B_CSR->nrows;
        const uint32_t B_ncols = B_CSR->ncols;
        const uint32_t* B_IA   = B_CSR->IA_blk->ptr;
        const uint32_t* B_JA   = B_CSR->JA_blk->ptr;
        const Weight*    B_A   = B_CSR->A_blk->ptr;
        
        if(A_ncols != B_nrows) {
            Logging::print(Logging::LOG_LEVEL::ERROR, "SpMM dimensions do not agree A[%d %d] B[%d %d]\n", A_nrows, A_ncols, B_nrows, B_ncols);
            std::exit(Env::finalize()); 
        }
                
        nrows = A_nrows;
        ncols = B_ncols;
        
        Logging::print(Logging::LOG_LEVEL::ERROR, "SpMM _CSR_ not implemented.\n");
        std::exit(Env::finalize()); 
    }
    else 
    */    
    if((A->compression_type == COMPRESSED_FORMAT::_CSC_) and (B->compression_type == COMPRESSED_FORMAT::_CSC_)) {
        const std::shared_ptr<struct CSC<Weight>> A_CSC = std::static_pointer_cast<struct CSC<Weight>>(A);
        const uint64_t A_nnz   = A_CSC->nnz;
        const uint32_t A_nrows = A_CSC->nrows;
        const uint32_t A_ncols = A_CSC->ncols;
        const uint32_t* A_IA   = A_CSC->IA_blk->ptr;
        const uint32_t* A_JA   = A_CSC->JA_blk->ptr;
        const Weight*    A_A   = A_CSC->A_blk->ptr;
        
        const std::shared_ptr<struct CSC<Weight>> B_CSC = std::static_pointer_cast<struct CSC<Weight>>(B);
        const uint64_t B_nnz   = B_CSC->nnz;
        const uint32_t B_nrows = B_CSC->nrows;
        const uint32_t B_ncols = B_CSC->ncols;
        const uint32_t* B_IA   = B_CSC->IA_blk->ptr;
        const uint32_t* B_JA   = B_CSC->JA_blk->ptr;
        const Weight*    B_A   = B_CSC->A_blk->ptr;
        //Logging::print(Logging::LOG_LEVEL::ERROR, "SpMM sym dimensions do not agree A[%d %d] B[%d %d]\n", A_nrows, A_ncols, B_nrows, B_ncols);
        if(A_ncols != B_nrows) {
            Logging::print(Logging::LOG_LEVEL::ERROR, "SpMM dimensions do not agree A[%d %d] B[%d %d]\n", A_nrows, A_ncols, B_nrows, B_ncols);
            std::exit(Env::finalize()); 
        }
        nrows = A_nrows;
        ncols = B_ncols;
        
        uint32_t start_col = Env::start_col[tid];
        uint32_t end_col = Env::end_col[tid];
        uint32_t displacement_nnz = Env::displacement_nnz[tid];
        
        for(uint32_t j = 0; j < B_ncols; j++) {
            for(uint32_t k = B_JA[j]; k < B_JA[j+1]; k++) {
                uint32_t l = B_IA[k];
                //l += (l == start_col) ? displacement_nnz : 0;
                uint32_t n = (l == start_col) ? displacement_nnz : 0;
                //uint32_t n = 0;
                //if(l == (end_col - 1))
                  //  printf("j=%d k=%d l=%d n=%d A[l]=%d A[l+1]=%d\n", j, k, l, n, A_JA[l], A_JA[l+1]);
                for(uint32_t m = A_JA[l] + n; m < A_JA[l+1]; m++) {
                    s[A_IA[m]] = 1;
                }
            }
            for(uint32_t i = 0; i < A_nrows; i++) {
                if(s[i]){
                    nnzmax++;
                    s[i] = 0;
                }
            }
        }

    }
    else {
        Logging::print(Logging::LOG_LEVEL::ERROR, "SpMM not implemented.\n");
        std::exit(Env::finalize()); 
    }
    return std::make_tuple(nnzmax, nrows, ncols);
}

template<typename Weight>
inline void spmm(std::shared_ptr<struct Compressed_Format<Weight>> A,
                 std::shared_ptr<struct Compressed_Format<Weight>> B,
                 std::shared_ptr<struct Compressed_Format<Weight>> C,
                 std::vector<Weight> s,
                 std::vector<Weight> b,
                 //const uint32_t B_start_col,
                 //const uint32_t B_end_col,
                 int32_t tid) {
                     
    if((A->compression_type == COMPRESSED_FORMAT::_CSC_) and 
       (B->compression_type == COMPRESSED_FORMAT::_CSC_) and
       (C->compression_type == COMPRESSED_FORMAT::_CSC_)) {  
       
        const std::shared_ptr<struct CSC<Weight>> A_CSC = std::static_pointer_cast<struct CSC<Weight>>(A);
        const uint64_t A_nnz   = A_CSC->nnz;
        const uint32_t A_nrows = A_CSC->nrows;
        const uint32_t A_ncols = A_CSC->ncols;
        const uint32_t* A_IA   = A_CSC->IA_blk->ptr;
        const uint32_t* A_JA   = A_CSC->JA_blk->ptr;
        const Weight*    A_A   = A_CSC->A_blk->ptr;
        
        const std::shared_ptr<struct CSC<Weight>> B_CSC = std::static_pointer_cast<struct CSC<Weight>>(B);
        const uint64_t B_nnz   = B_CSC->nnz;
        const uint32_t B_nrows = B_CSC->nrows;
        const uint32_t B_ncols = B_CSC->ncols;
        const uint32_t* B_IA   = B_CSC->IA_blk->ptr;
        const uint32_t* B_JA   = B_CSC->JA_blk->ptr;
        const Weight*    B_A   = B_CSC->A_blk->ptr;
        //const uint32_t start = B_CSC->start_col;
        //const uint32_t end = B_CSC->end_col;
        
        const std::shared_ptr<struct CSC<Weight>> C_CSC = std::static_pointer_cast<struct CSC<Weight>>(C);
        const uint64_t C_nnz   = C_CSC->nnz;
        const uint32_t C_nrows = C_CSC->nrows;
        const uint32_t C_ncols = C_CSC->ncols;
        const uint32_t* C_IA   = C_CSC->IA_blk->ptr;
        uint32_t* C_JA   = C_CSC->JA_blk->ptr;
        const Weight*    C_A   = C_CSC->A_blk->ptr;
        
        uint32_t start_col = Env::start_col[tid];
        uint32_t end_col = Env::end_col[tid];
        uint32_t displacement_nnz = Env::displacement_nnz[tid];
        
        //uint64_t& offset_nnz = Env::offset_nnz[tid];
        //uint64_t& index_nnz = Env::index_nnz[tid];
        //uint64_t index_nnz = offset_nnz;
        //C_JA[B_start_col] = Env::offset_nnz[tid];
        //printf("B_start_col=%d, C_JA[B_start_col]= %d\n", B_start_col, C_JA[B_start_col]);
        
        if(A_ncols != B_nrows) {
            Logging::print(Logging::LOG_LEVEL::ERROR, "SpMM dimensions do not agree C[%d %d] != A[%d %d] B[%d %d]\n", C_nrows, C_ncols, A_nrows, A_ncols, B_nrows, B_ncols);
            std::exit(Env::finalize()); 
        }
        
        for(uint32_t j = 0; j < B_ncols; j++) {
            for(uint32_t k = B_JA[j]; k < B_JA[j+1]; k++) {
                uint32_t l = B_IA[k];
                //uint32_t n = (l == (end_col - 1)) ? displacement_nnz : 0;
                //uint32_t n = 0;
                uint32_t n = (l == start_col) ? displacement_nnz : 0;
                //if(l == (end_col - 1))
                    //printf("%d %d %d %d %d\n", j, k, l, n, A_JA[l]);
                for(uint32_t m = A_JA[l] + n; m < A_JA[l+1]; m++) {
                    s[A_IA[m]] += (B_A[k] * A_A[m]);
                }
            }
            //C_CSC->populate_spa(s, b, j);
            //j += (tid == 0) ? 1 : 0;
            C_CSC->populate_spa_t(s, b, j, tid);
        }
        //printf("%d %lu %lu %lu\n", tid, index_nnz, Env::offset_nnz[tid], index_nnz - Env::offset_nnz[tid]);
        //C_CSC->adjust();
        //Env::index_nnz[tid] = index_nnz;
        #pragma omp barrier
        //C_CSC->refine_t(B_start_col, B_end_col, tid);
        //if(!tid) 
        C_CSC->adjust(tid);
       // C_CSC->walk_t(tid);
        
   }
   else {
        Logging::print(Logging::LOG_LEVEL::ERROR, "SpMM not implemented.\n");
        std::exit(Env::finalize()); 
   }
}


template<typename Weight>
inline char validate_prediction(std::shared_ptr<struct Compressed_Format<Weight>> A,
                                      std::vector<uint32_t> trueCategories) {
    const std::shared_ptr<struct CSC<Weight>> A_CSC = std::static_pointer_cast<struct CSC<Weight>>(A);
    const uint64_t A_nnz   = A_CSC->nnz;
    const uint32_t A_nrows = A_CSC->nrows;
    const uint32_t A_ncols = A_CSC->ncols;
    const uint32_t* A_IA   = A_CSC->IA_blk->ptr;
    const uint32_t* A_JA   = A_CSC->JA_blk->ptr;
    const Weight*    A_A   = A_CSC->A_blk->ptr;
    
    
    
    
    std::vector<uint32_t> allCategories(A_nrows);
    for(int32_t t = 0; t < Env::nthreads; t++) {
        uint32_t start_col = Env::start_col[t];
        uint32_t end_col = Env::end_col[t];
        uint32_t displacement_nnz = Env::displacement_nnz[t];
        for(uint32_t j = start_col; j < end_col; j++) {
            uint32_t n = (j == start_col) ? displacement_nnz : 0;
            //uint32_t n = (j == (end_col-1)) ? displacement_nnz : 0;
            //if(i < 1000)
            //printf("t=%d j=%d n=%d d=%d[%d %d %d]\n", t, j, n, displacement_nnz, A_JA[j], A_JA[j+1], A_JA[j+1] - A_JA[j] - n);
            for(uint32_t i = A_JA[j] + n ; i < A_JA[j+1]; i++) {
                allCategories[A_IA[i]] = 1;
            }
        }
    }
    /*
    std::vector<uint32_t> allCategories(A_nrows);
    for(uint32_t j = 0; j < A_ncols; j++) {
        uint32_t l = (j == start_col) ? displacement_nnz : 0;
        for(uint32_t i = A_JA[j]; i < A_JA[j+1]; i++) {
            allCategories[A_IA[i]] = 1;
        }
    }
    */
    
    char me = 1;
    uint32_t j = 0;
    for(uint32_t i = 0; i < A_nrows; i++) {
        //if(i < 1000)
        
        if(trueCategories[i] != allCategories[i]) {
            me = 0;
            //break;
        }
        //else {
          //printf("%d %d %d\n", i, trueCategories[i], allCategories[i]);  
        //}
        //if(i == 1000) break;
    }
    char all = 0;
    MPI_Allreduce(&me, &all, 1, MPI_CHAR, MPI_SUM, MPI_COMM_WORLD);

    return((all == Env::nranks));
}

#endif