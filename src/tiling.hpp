/*
 * tiling.hpp: Tiling strategy
 * (c) Mohammad Hasanzadeh Mofrad, 2019
 * (e) m.hasanzadeh.mofrad@gmail.com 
 */

#ifndef TILING_HPP
#define TILING_HPP

#include <fstream>
#include <sstream>
#include <numeric>
#include<tuple>

#include "triple.hpp"
#include "tile.hpp"
#include "io.hpp"
#include "allocator.hpp"
#include "spmat.hpp"

enum TILING_TYPE {_1D_COL_, _1D_ROW_, _2D_};
const char* TILING_TYPES[] = {"_1D_COL_", "_1D_ROW_", "_2D_"};

template<typename Weight>
class Tiling {
    public:
        Tiling() {};
        ~Tiling() {};
        
        Tiling(const uint32_t ntiles_, const uint32_t nrowgrps_, const uint32_t ncolgrps_, const uint32_t nranks_, 
               const uint64_t nnz_, const uint32_t nrows_, const uint32_t ncols_, 
               const std::string input_file, const INPUT_TYPE input_type, 
               const TILING_TYPE tiling_type_, const bool repartition = false);

        Tiling(const uint32_t ntiles_, const uint32_t nrowgrps_, const uint32_t ncolgrps_, 
               const uint32_t nranks_, const uint32_t rank_nthreads_, const uint32_t nthreads_,
               const uint64_t nnz_, const uint32_t nrows_, const uint32_t ncols_, 
               const std::string input_file, const INPUT_TYPE input_type, 
               const TILING_TYPE tiling_type_, const bool repartition = false);               

        Tiling(const uint32_t ntiles_, const uint32_t nrowgrps_, const uint32_t ncolgrps_, const uint32_t nranks_, 
               const uint64_t nnz_, const uint32_t nrows_, const uint32_t ncols_, 
               const TILING_TYPE tiling_type_, const bool repartition = false);
                
        Tiling(const uint32_t ntiles_, const uint32_t nrowgrps_, const uint32_t ncolgrps_, const uint32_t nranks_,
               const uint32_t rank_nthreads_, const uint32_t nthreads_, 
               const uint64_t nnz_, const uint32_t nrows_, const uint32_t ncols_,
               const TILING_TYPE tiling_type_, const bool repartition = false);

        uint32_t ntiles, nrowgrps, ncolgrps;
        uint32_t nranks, rank_ntiles, rank_nrowgrps, rank_ncolgrps;
        uint32_t rowgrp_nranks, colgrp_nranks;
        uint32_t rank_nthreads;
        
        uint32_t threads_ntiles, threads_nrowgrps, threads_ncolgrps;
        uint32_t nthreads, thread_ntiles, thread_nrowgrps, thread_ncolgrps;
        uint32_t rowgrp_nthreads, colgrp_nthreads;
        
        uint64_t nnz;        
        uint32_t nrows, ncols;
        
        uint32_t tile_height, tile_width;
        TILING_TYPE tiling_type;
        
        std::vector<std::vector<struct Tile<Weight>>> tiles;
        std::vector<uint32_t> bounds;
        
        bool one_rank = false;
        void set_threads_indices();
        uint32_t get_tile_info(const std::string field, const int32_t tid);
        void     set_tile_info(const std::vector<std::vector<struct Tile<Weight>>> other_tiles); 
        void test();
        void test1();

    private:
        void integer_factorize(const uint32_t n, uint32_t& a, uint32_t& b);
        void populate_tiling();
        void print_tiling(const std::string field);
        bool assert_tiling();
        
        void exchange_triples();
        void insert_triples(std::vector<struct Triple<Weight>>& triples);
        void delete_triples(std::vector<struct Triple<Weight>>& triples);
        void compress_triples();
        
        void repartition_tiles(const std::string input_file, const INPUT_TYPE input_type);
        
        void tile_load();
        void tile_load_print(const std::vector<uint64_t> nedges_vec, const uint64_t nedges, const uint32_t nedges_divisor, const std::string nedges_type);
};

/* Process-based tiling based on MPI ranks*/ 
template<typename Weight>
Tiling<Weight>::Tiling(const uint32_t ntiles_, const uint32_t nrowgrps_, const uint32_t ncolgrps_, const uint32_t nranks_, 
                       const uint64_t nnz_, const uint32_t nrows_, const uint32_t ncols_,
                       const std::string input_file, const INPUT_TYPE input_type,
                       const TILING_TYPE tiling_type_,
                       const bool repartition) 
        : ntiles(ntiles_) , nrowgrps(nrowgrps_), ncolgrps(ncolgrps_), nranks(nranks_), rank_ntiles(ntiles_/nranks_), 
          nnz(nnz_), nrows(nrows_), ncols(ncols_), tiling_type(tiling_type_) {
            
    one_rank = ((nranks == 1) and (nranks != (uint32_t) Env::nranks)) ? true : false;
   
    if((rank_ntiles * nranks != ntiles) or (nrowgrps * ncolgrps != ntiles)) {
        Logging::print(Logging::LOG_LEVEL::ERROR, "Tiling failed\n");
        std::exit(Env::finalize()); 
    }
    
    if (tiling_type == TILING_TYPE::_1D_ROW_) {
        rowgrp_nranks = 1;
        colgrp_nranks = nranks;
        if(rowgrp_nranks * colgrp_nranks != nranks) {
            Logging::print(Logging::LOG_LEVEL::ERROR, "Tiling failed\n");
            std::exit(Env::finalize()); 
        }
        nrows += (nrows % nrowgrps) ? (nrowgrps - (nrows % nrowgrps)) : 0;
    }
    else if(tiling_type == TILING_TYPE::_1D_COL_) {
        rowgrp_nranks = nranks;
        colgrp_nranks = 1;
        if(rowgrp_nranks * colgrp_nranks != nranks) {
            Logging::print(Logging::LOG_LEVEL::ERROR, "Tiling failed\n");
            std::exit(Env::finalize()); 
        }
        ncols += (ncols % ncolgrps) ? (ncolgrps - (ncols % ncolgrps)) : 0;
    }

    rank_nrowgrps = nrowgrps / colgrp_nranks;
    rank_ncolgrps = ncolgrps / rowgrp_nranks;        
    if(rank_nrowgrps * rank_ncolgrps != rank_ntiles) {
        Logging::print(Logging::LOG_LEVEL::ERROR, "Tiling failed\n");
        std::exit(Env::finalize()); 
    }
    
    nthreads = 1;//Env::nthreads; // I changed this in the past
    if (tiling_type == TILING_TYPE::_1D_ROW_) {
        rowgrp_nthreads = 1;
        colgrp_nthreads = nthreads;
        
    }
    else if (tiling_type == TILING_TYPE::_1D_COL_) {
        rowgrp_nthreads =  nthreads;
        colgrp_nthreads = 1;
        
    }

    if(rowgrp_nthreads * colgrp_nthreads != nthreads) {
        Logging::print(Logging::LOG_LEVEL::ERROR, "Tiling failed\n");
        std::exit(Env::finalize()); 
    }
    
    threads_nrowgrps = nrowgrps;
    threads_ncolgrps = ncolgrps;
    
    thread_ntiles = ntiles/nthreads;
    
    thread_nrowgrps = threads_nrowgrps / colgrp_nthreads;
    thread_ncolgrps = threads_ncolgrps / rowgrp_nthreads;
    if(thread_nrowgrps * thread_ncolgrps != thread_ntiles) {
        Logging::print(Logging::LOG_LEVEL::ERROR, "Tiling failed\n");
        std::exit(Env::finalize()); 
    }
    
    tile_height = nrows / nrowgrps;
    tile_width  = ncols / ncolgrps;
    
    tiles.resize(nrowgrps);
    for (uint32_t i = 0; i < nrowgrps; i++) {
        tiles[i].resize(ncolgrps);
    }
    
    int32_t gcd_r = std::gcd(rowgrp_nranks, colgrp_nranks);
    for (uint32_t i = 0; i < nrowgrps; i++) {
        for (uint32_t j = 0; j < ncolgrps; j++) {
            auto& tile = tiles[i][j];
            tile.rank = (((i % colgrp_nranks) * rowgrp_nranks + (j % rowgrp_nranks)) + ((i / (nrowgrps/(gcd_r))) * (rank_nrowgrps))) % nranks;
            tile.thread = 0;
            tile.start_row = i*tile_height;
            tile.end_row = (i+1)*tile_height;
            tile.height = tile_height;
            tile.start_col = j*tile_width;
            tile.end_col = (j+1)*tile_width;
            tile.width = tile_width;
        }
    }
    
    if (tiling_type == TILING_TYPE::_1D_ROW_) {
        bounds.resize(nrowgrps);
        for (uint32_t i = 0; i < nrowgrps; i++) {
            bounds[i] = tiles[i][0].end_row;
        }
    }
    else if (tiling_type == TILING_TYPE::_1D_COL_) {
        bounds.resize(ncolgrps);
        for (uint32_t j = 0; j < ncolgrps; j++) {
            bounds[j] = tiles[0][j].end_col;
        }
    }

    if(one_rank) {
        for (uint32_t i = 0; i < nrowgrps; i++) {
            for (uint32_t j = 0; j < ncolgrps; j++) {
                tiles[i][j].rank = Env::rank;
            }
        }
    }
    
    if((not one_rank) and (ntiles == nranks *nranks)) {
        if(not assert_tiling()) {
            Logging::print(Logging::LOG_LEVEL::ERROR, "Tiling failed1\n");
            std::exit(Env::finalize()); 
        }
    }
    
    Logging::print(Logging::LOG_LEVEL::INFO, "Tiling information: Process-based%s\n", TILING_TYPES[tiling_type]);
    Logging::print(Logging::LOG_LEVEL::INFO, "Tiling information: nrowgrps      x ncolgrps      = [%d x %d]\n", nrowgrps, ncolgrps);
    Logging::print(Logging::LOG_LEVEL::INFO, "Tiling information: rowgrp_nranks x colgrp_nranks = [%d x %d]\n", rowgrp_nranks, colgrp_nranks);
    Logging::print(Logging::LOG_LEVEL::INFO, "Tiling information: rank_nrowgrps x rank_ncolgrps = [%d x %d]\n", rank_nrowgrps, rank_ncolgrps);
    Logging::print(Logging::LOG_LEVEL::INFO, "Tiling information: nrows         x ncols         = [%d x %d]\n", nrows, ncols);
    Logging::print(Logging::LOG_LEVEL::INFO, "Tiling information: tile_height   x tile_width    = [%d x %d]\n", tile_height, tile_width);
    Logging::print(Logging::LOG_LEVEL::INFO, "Tiling information: nnz                           = [%d]\n", nnz);
    
    std::vector<struct Triple<Weight>> triples;
    if(INPUT_TYPE::_TEXT_ == input_type) {
        triples = IO::text_file_read<Weight>(input_file, one_rank);
    }
    else {
        triples = IO::binary_file_read<Weight>(input_file, one_rank);
    }
    insert_triples(triples);
    delete_triples(triples);
    
    if(not one_rank) {
        exchange_triples();
        tile_load();
    }
    else {
        for (uint32_t i = 0; i < nrowgrps; i++) {
            for (uint32_t j = 0; j < ncolgrps; j++) {
                auto& tile = tiles[i][j];
                if(tile.rank == Env::rank) {
                    tiles[i][j].nedges = tiles[i][j].triples.size();
                }
            }
        }
    }
    
    print_tiling("rank");
    print_tiling("nedges");

    if(repartition) {        
        repartition_tiles(input_file, input_type);
        print_tiling("nedges");
        print_tiling("height");
    }
    compress_triples(); 
}

template<typename Weight>
Tiling<Weight>::Tiling(const uint32_t ntiles_, const uint32_t nrowgrps_, const uint32_t ncolgrps_,  const uint32_t nranks_, 
                       const uint32_t rank_nthreads_, const uint32_t nthreads_, 
                       const uint64_t nnz_, const uint32_t nrows_, const uint32_t ncols_,
                       const std::string input_file, const INPUT_TYPE input_type,
                       const TILING_TYPE tiling_type_, 
                       const bool repartition)
                     : ntiles(ntiles_) , nrowgrps(nrowgrps_), ncolgrps(ncolgrps_), nranks(nranks_), rank_ntiles(ntiles_/nranks_), 
                       rank_nthreads(rank_nthreads_), nthreads(nthreads_),
                       nnz(nnz_), nrows(nrows_), ncols(ncols_), tiling_type(tiling_type_) {
    
    one_rank = ((nranks == 1) and (nranks != (uint32_t) Env::nranks)) ? true : false;              
    
    if((rank_ntiles * nranks != ntiles) or (nrowgrps * ncolgrps != ntiles)) {
        Logging::print(Logging::LOG_LEVEL::ERROR, "Tiling failed\n");
        std::exit(Env::finalize()); 
    }
    
    if (tiling_type == TILING_TYPE::_1D_ROW_) {
        rowgrp_nranks = 1;
        colgrp_nranks = nranks;
        if(rowgrp_nranks * colgrp_nranks != nranks) {
            Logging::print(Logging::LOG_LEVEL::ERROR, "Tiling failed 1\n");
            std::exit(Env::finalize()); 
        }
        nrows += (nrows % nrowgrps) ? (nrowgrps - (nrows % nrowgrps)) : 0;
    }
    else if(tiling_type == TILING_TYPE::_1D_COL_) {
        rowgrp_nranks = nranks;
        colgrp_nranks = 1;
        if(rowgrp_nranks * colgrp_nranks != nranks) {
            Logging::print(Logging::LOG_LEVEL::ERROR, "Tiling failed\n");
            std::exit(Env::finalize()); 
        }
        ncols += (ncols % ncolgrps) ? (ncolgrps - (ncols % ncolgrps)) : 0;
    }

    rank_nrowgrps = nrowgrps / colgrp_nranks;
    rank_ncolgrps = ncolgrps / rowgrp_nranks;        
    if(rank_nrowgrps * rank_ncolgrps != rank_ntiles) {
        Logging::print(Logging::LOG_LEVEL::ERROR, "Tiling failedd\n");
        std::exit(Env::finalize()); 
    }
    
    threads_ntiles = nthreads;
    thread_ntiles = threads_ntiles/nthreads;
    
    if (tiling_type == TILING_TYPE::_1D_ROW_) {
        threads_nrowgrps = nthreads;
        threads_ncolgrps = 1;
    }
    else if(tiling_type == TILING_TYPE::_1D_COL_) {
        threads_nrowgrps = 1;
        threads_ncolgrps = nthreads;
    }

    if((thread_ntiles * nthreads != threads_ntiles) or (threads_nrowgrps * threads_ncolgrps != threads_ntiles)) {
        Logging::print(Logging::LOG_LEVEL::ERROR, "Tiling failed\n");
        std::exit(Env::finalize()); 
    }

    if (tiling_type == TILING_TYPE::_1D_ROW_) {
        rowgrp_nthreads = 1;
        colgrp_nthreads = nthreads;
    }
    else if (tiling_type == TILING_TYPE::_1D_COL_) {
        rowgrp_nthreads =  nthreads;
        colgrp_nthreads = 1;
    }

    if(rowgrp_nthreads * colgrp_nthreads != nthreads) {
        Logging::print(Logging::LOG_LEVEL::ERROR, "Tiling failed\n");
        std::exit(Env::finalize()); 
    }
    
    thread_nrowgrps = threads_nrowgrps / colgrp_nthreads;
    thread_ncolgrps = threads_ncolgrps / rowgrp_nthreads;
    if(thread_nrowgrps * thread_ncolgrps != thread_ntiles) {
        Logging::print(Logging::LOG_LEVEL::ERROR, "Tiling failed\n");
        std::exit(Env::finalize()); 
    }

    tile_height = nrows / nrowgrps;
    tile_width  = ncols / ncolgrps;
    
    tiles.resize(nrowgrps);
    for (uint32_t i = 0; i < nrowgrps; i++) {
        tiles[i].resize(ncolgrps);
    }
    
    int32_t gcd_r = std::gcd(rowgrp_nranks, colgrp_nranks);
    int32_t gcd_t = std::gcd(rowgrp_nthreads, colgrp_nthreads);

    for (uint32_t i = 0; i < nrowgrps; i++) {
        for (uint32_t j = 0; j < ncolgrps; j++) {
            auto& tile = tiles[i][j];
            int32_t thread_rank = (((i % colgrp_nthreads) * rowgrp_nthreads + (j % rowgrp_nthreads)) 
                                + ((i / (nrowgrps/gcd_t)) * (thread_nrowgrps))) % (Env::nranks * Env::nthreads);
            tile.rank   = thread_rank % Env::nranks;
            tile.thread = thread_rank / Env::nranks;   
            tile.start_row = i*tile_height;
            tile.end_row = (i+1)*tile_height;
            tile.height = tile_height;
            tile.start_col = j*tile_width;
            tile.end_col = (j+1)*tile_width;
            tile.width = tile_width;        
        }
    }
    
    if (tiling_type == TILING_TYPE::_1D_ROW_) {
        bounds.resize(nrowgrps);
        for (uint32_t i = 0; i < nrowgrps; i++) {
            bounds[i] = tiles[i][0].end_row;
        }
    }
    else if (tiling_type == TILING_TYPE::_1D_COL_) {
        bounds.resize(ncolgrps);
        for (uint32_t j = 0; j < ncolgrps; j++) {
            bounds[j] = tiles[0][j].end_col;
        }
    }
    
    if(one_rank) {
        for (uint32_t i = 0; i < nrowgrps; i++) {
            for (uint32_t j = 0; j < ncolgrps; j++) {
                tiles[i][j].rank = Env::rank;
            }
        }
    }
    
    Logging::print(Logging::LOG_LEVEL::INFO, "Tiling Information: Thread-based%s\n", TILING_TYPES[tiling_type]);
    Logging::print(Logging::LOG_LEVEL::INFO, "Tiling information: nrowgrps      x ncolgrps      = [%d x %d]\n", nrowgrps, ncolgrps);
    Logging::print(Logging::LOG_LEVEL::INFO, "Tiling information: rowgrp_nranks x colgrp_nranks = [%d x %d]\n", rowgrp_nranks, colgrp_nranks);
    Logging::print(Logging::LOG_LEVEL::INFO, "Tiling information: rank_nrowgrps x rank_ncolgrps = [%d x %d]\n", rank_nrowgrps, rank_ncolgrps);
    Logging::print(Logging::LOG_LEVEL::INFO, "Tiling Information: rowgrp_nthreads  x colgrp_nthreads  = [%d x %d]\n", rowgrp_nthreads, colgrp_nthreads);
    Logging::print(Logging::LOG_LEVEL::INFO, "Tiling Information: thread_nrowgrps  x thread_ncolgrps  = [%d x %d]\n", thread_nrowgrps, thread_ncolgrps);
    Logging::print(Logging::LOG_LEVEL::INFO, "Tiling information: nrows            x ncols            = [%d x %d]\n", nrows, ncols);
    Logging::print(Logging::LOG_LEVEL::INFO, "Tiling information: tile_height      x tile_width       = [%d x %d]\n", tile_height, tile_width);
    Logging::print(Logging::LOG_LEVEL::INFO, "Tiling information: nnz                                 = [%d]\n", nnz);
    
    std::vector<struct Triple<Weight>> triples;
    if(INPUT_TYPE::_TEXT_ == input_type) {
        triples = IO::text_file_read<Weight>(input_file, one_rank);
    }
    else {
        triples = IO::binary_file_read<Weight>(input_file, one_rank);
    }
    insert_triples(triples);
    delete_triples(triples);
    
    if(not one_rank) {
        exchange_triples();
        tile_load();
    }
    else {
        for (uint32_t i = 0; i < nrowgrps; i++) {
            for (uint32_t j = 0; j < ncolgrps; j++) {
                auto& tile = tiles[i][j];
                if(tile.rank == Env::rank) {
                    tiles[i][j].nedges = tiles[i][j].triples.size();
                }
            }
        }
    }
    
    print_tiling("rank");
    print_tiling("thread");
    print_tiling("nedges");
   
    if(repartition) {
        repartition_tiles(input_file, input_type);
        print_tiling("nedges");
        print_tiling("width");
    }

    compress_triples();
    //test();    
    //test1();
}


template<typename Weight>
Tiling<Weight>::Tiling(const uint32_t ntiles_, const uint32_t nrowgrps_, const uint32_t ncolgrps_, const uint32_t nranks_, 
                       const uint32_t rank_nthreads_, const uint32_t nthreads_, 
                       const uint64_t nnz_, const uint32_t nrows_, const uint32_t ncols_,
                       const TILING_TYPE tiling_type_, const bool repartition)
                     : ntiles(ntiles_), nrowgrps(nrowgrps_), ncolgrps(ncolgrps_), nranks(nranks_), rank_ntiles(ntiles_/nranks_), 
                       nthreads(nthreads_),
                       nnz(nnz_), nrows(nrows_), ncols(ncols_), tile_height(nrows / nrowgrps), tile_width(ncols / ncolgrps), tiling_type(tiling_type_) {
    if((rank_ntiles * nranks != ntiles) or (nrowgrps * ncolgrps != ntiles)) {
        Logging::print(Logging::LOG_LEVEL::ERROR, "Tiling failed\n");
        std::exit(Env::finalize()); 
    }
    
    if (tiling_type == TILING_TYPE::_1D_ROW_) {
        rowgrp_nranks = 1;
        colgrp_nranks = nranks;
        if(rowgrp_nranks * colgrp_nranks != nranks) {
            Logging::print(Logging::LOG_LEVEL::ERROR, "Tiling failed\n");
            std::exit(Env::finalize()); 
        }
    }
    else {
        Logging::print(Logging::LOG_LEVEL::ERROR, "Tiling failed\n");
        std::exit(Env::finalize()); 
    }
    
    rank_nrowgrps = nrowgrps / colgrp_nranks;
    rank_ncolgrps = ncolgrps / rowgrp_nranks;        
    if(rank_nrowgrps * rank_ncolgrps != rank_ntiles) {
        Logging::print(Logging::LOG_LEVEL::ERROR, "Tiling failed\n");
        std::exit(Env::finalize()); 
    }

    threads_ntiles = nthreads;
    thread_ntiles = threads_ntiles/nthreads;
    
    if (tiling_type == TILING_TYPE::_1D_ROW_) {
        threads_nrowgrps = nthreads;
        threads_ncolgrps = 1;
    }
    else if(tiling_type == TILING_TYPE::_1D_COL_) {
        threads_nrowgrps = 1;
        threads_ncolgrps = nthreads;
    }

    if((thread_ntiles * nthreads != threads_ntiles) or (threads_nrowgrps * threads_ncolgrps != threads_ntiles)) {
        Logging::print(Logging::LOG_LEVEL::ERROR, "Tiling failed\n");
        std::exit(Env::finalize()); 
    }

    if (tiling_type == TILING_TYPE::_1D_ROW_) {
        rowgrp_nthreads = 1;
        colgrp_nthreads = nthreads;
    }
    else if (tiling_type == TILING_TYPE::_1D_COL_) {
        rowgrp_nthreads =  nthreads;
        colgrp_nthreads = 1;
    }

    if(rowgrp_nthreads * colgrp_nthreads != nthreads) {
        Logging::print(Logging::LOG_LEVEL::ERROR, "Tiling failed\n");
        std::exit(Env::finalize()); 
    }
    thread_nrowgrps = threads_nrowgrps / colgrp_nthreads;
    thread_ncolgrps = threads_ncolgrps / rowgrp_nthreads;
    if(thread_nrowgrps * thread_ncolgrps != thread_ntiles) {
        Logging::print(Logging::LOG_LEVEL::ERROR, "Tiling failed\n");
        std::exit(Env::finalize()); 
    }
    
    tiles.resize(nrowgrps);
    for (uint32_t i = 0; i < nrowgrps; i++) {
        tiles[i].resize(ncolgrps);
    }
    
    int32_t gcd_r = std::gcd(rowgrp_nranks, colgrp_nranks);
    int32_t gcd_t = std::gcd(rowgrp_nthreads, colgrp_nthreads);
    for (uint32_t i = 0; i < nrowgrps; i++) {
        for (uint32_t j = 0; j < ncolgrps; j++) {
            auto& tile = tiles[i][j];
            int32_t thread_rank = (((i % colgrp_nthreads) * rowgrp_nthreads + (j % rowgrp_nthreads)) 
                               + ((i / (nrowgrps/gcd_t)) * (thread_nrowgrps))) % (Env::nranks * Env::nthreads);
            tile.rank   = thread_rank % Env::nranks;
            tile.thread = thread_rank / Env::nranks;  
            tile.start_row = i*tile_height;
            tile.end_row = (i+1)*tile_height;
            tile.height = tile_height;
            tile.start_col = j*tile_width;
            tile.end_col = (j+1)*tile_width;
            tile.width = tile_width;   
        }
    }
    
    Logging::print(Logging::LOG_LEVEL::INFO, "Tiling Information: Thread-based%s\n", TILING_TYPES[tiling_type]);
    Logging::print(Logging::LOG_LEVEL::INFO, "Tiling information: nrowgrps      x ncolgrps      = [%d x %d]\n", nrowgrps, ncolgrps);
    Logging::print(Logging::LOG_LEVEL::INFO, "Tiling information: rowgrp_nranks x colgrp_nranks = [%d x %d]\n", rowgrp_nranks, colgrp_nranks);
    Logging::print(Logging::LOG_LEVEL::INFO, "Tiling information: rank_nrowgrps x rank_ncolgrps = [%d x %d]\n", rank_nrowgrps, rank_ncolgrps);
    Logging::print(Logging::LOG_LEVEL::INFO, "Tiling Information: rowgrp_nthreads  x colgrp_nthreads  = [%d x %d]\n", rowgrp_nthreads, colgrp_nthreads);
    Logging::print(Logging::LOG_LEVEL::INFO, "Tiling Information: thread_nrowgrps  x thread_ncolgrps  = [%d x %d]\n", thread_nrowgrps, thread_ncolgrps);
    Logging::print(Logging::LOG_LEVEL::INFO, "Tiling information: nrows            x ncols            = [%d x %d]\n", nrows, ncols);
    Logging::print(Logging::LOG_LEVEL::INFO, "Tiling information: tile_height      x tile_width       = [%d x %d]\n", tile_height, tile_width);
    Logging::print(Logging::LOG_LEVEL::INFO, "Tiling information: nnz                                 = [%d]\n", nnz);
    
    for (uint32_t i = 0; i < nrowgrps; i++) {
        for (uint32_t j = 0; j < ncolgrps; j++) {
            tiles[i][j].nedges = 0;
        }
    }

    print_tiling("rank");
    print_tiling("thread");
    print_tiling("nedges");

    compress_triples();  
}


template<typename Weight>
Tiling<Weight>::Tiling(const uint32_t ntiles_, const uint32_t nrowgrps_, const uint32_t ncolgrps_, const uint32_t nranks_, 
                       const uint64_t nnz_, const uint32_t nrows_, const uint32_t ncols_,
                       const TILING_TYPE tiling_type_, const bool repartition)
                     : ntiles(ntiles_), nrowgrps(nrowgrps_), ncolgrps(ncolgrps_), nranks(nranks_), rank_ntiles(ntiles_/nranks_), 
                       nnz(nnz_), nrows(nrows_), ncols(ncols_), tile_height(nrows / nrowgrps), tile_width(ncols / ncolgrps), tiling_type(tiling_type_) {
                           
    if((rank_ntiles * nranks != ntiles) or (nrowgrps * ncolgrps != ntiles)) {
        Logging::print(Logging::LOG_LEVEL::ERROR, "Tiling failed\n");
        std::exit(Env::finalize()); 
    }
    
    if (tiling_type == TILING_TYPE::_1D_ROW_) {
        rowgrp_nranks = 1;
        colgrp_nranks = nranks;
        if(rowgrp_nranks * colgrp_nranks != nranks) {
            Logging::print(Logging::LOG_LEVEL::ERROR, "Tiling failed\n");
            std::exit(Env::finalize()); 
        }
    }
    else {
        Logging::print(Logging::LOG_LEVEL::ERROR, "Tiling failed\n");
        std::exit(Env::finalize()); 
    }
    
    rank_nrowgrps = nrowgrps / colgrp_nranks;
    rank_ncolgrps = ncolgrps / rowgrp_nranks;        
    if(rank_nrowgrps * rank_ncolgrps != rank_ntiles) {
        Logging::print(Logging::LOG_LEVEL::ERROR, "Tiling failed\n");
        std::exit(Env::finalize()); 
    }
    
    threads_ntiles = nthreads;
    thread_ntiles = threads_ntiles/nthreads;
    threads_nrowgrps = nthreads;
    threads_ncolgrps = 1;

    if((thread_ntiles * nthreads != threads_ntiles) or (threads_nrowgrps * threads_ncolgrps != threads_ntiles)) {
        Logging::print(Logging::LOG_LEVEL::ERROR, "Tiling failed\n");
        std::exit(Env::finalize()); 
    }

    if (tiling_type == TILING_TYPE::_1D_ROW_) {
        rowgrp_nthreads = 1;
        colgrp_nthreads = nthreads;
    }
    else if (tiling_type == TILING_TYPE::_1D_COL_) {
        rowgrp_nthreads =  nthreads;
        colgrp_nthreads = 1;
    }

    if(rowgrp_nthreads * colgrp_nthreads != nthreads) {
        Logging::print(Logging::LOG_LEVEL::ERROR, "Tiling failed\n");
        std::exit(Env::finalize()); 
    }
    thread_nrowgrps = threads_nrowgrps / colgrp_nthreads;
    thread_ncolgrps = threads_ncolgrps / rowgrp_nthreads;
    if(thread_nrowgrps * thread_ncolgrps != thread_ntiles) {
        Logging::print(Logging::LOG_LEVEL::ERROR, "Tiling failed\n");
        std::exit(Env::finalize()); 
    }

    tiles.resize(nrowgrps);
    for (uint32_t i = 0; i < nrowgrps; i++) {
        tiles[i].resize(ncolgrps);
    }

    int32_t gcd_r = std::gcd(rowgrp_nranks, colgrp_nranks);
    for (uint32_t i = 0; i < nrowgrps; i++) {
        for (uint32_t j = 0; j < ncolgrps; j++) {
            auto& tile = tiles[i][j];
            tile.rank = (((i % colgrp_nranks) * rowgrp_nranks + (j % rowgrp_nranks)) + ((i / (nrowgrps/(gcd_r))) * (rank_nrowgrps))) % nranks;
            tile.start_row = i*tile_height;
            tile.end_row = (i+1)*tile_height;
            tile.height = tile_height;
            tile.start_col = j*tile_width;
            tile.end_col = (j+1)*tile_width;
            tile.width = tile_width;
        }
    }
    
    Logging::print(Logging::LOG_LEVEL::INFO, "Tiling Information: Thread-based%s\n", TILING_TYPES[tiling_type]);
    Logging::print(Logging::LOG_LEVEL::INFO, "Tiling information: nrowgrps      x ncolgrps      = [%d x %d]\n", nrowgrps, ncolgrps);
    Logging::print(Logging::LOG_LEVEL::INFO, "Tiling information: rowgrp_nranks x colgrp_nranks = [%d x %d]\n", rowgrp_nranks, colgrp_nranks);
    Logging::print(Logging::LOG_LEVEL::INFO, "Tiling information: rank_nrowgrps x rank_ncolgrps = [%d x %d]\n", rank_nrowgrps, rank_ncolgrps);
    Logging::print(Logging::LOG_LEVEL::INFO, "Tiling information: nrows            x ncols            = [%d x %d]\n", nrows, ncols);
    Logging::print(Logging::LOG_LEVEL::INFO, "Tiling information: tile_height      x tile_width       = [%d x %d]\n", tile_height, tile_width);
    Logging::print(Logging::LOG_LEVEL::INFO, "Tiling information: nnz                                 = [%d]\n", nnz);
    
    for (uint32_t i = 0; i < nrowgrps; i++) {
        for (uint32_t j = 0; j < ncolgrps; j++) {
            tiles[i][j].nedges = 0;
        }
    }
    
    print_tiling("rank");
    print_tiling("nedges");

    compress_triples();  
}

template<typename Weight>
void Tiling<Weight>::set_threads_indices() {
    for (uint32_t i = 0; i < nrowgrps; i++) {
        for (uint32_t j = 0; j < ncolgrps; j++) {
            auto& tile = tiles[i][j];
            if(tile.rank == Env::rank) {
                Env::thread_rowgroup[tile.thread] = i;
            }
        }
    } 
}

template<typename Weight>
void Tiling<Weight>::integer_factorize(const uint32_t n, uint32_t& a, uint32_t& b) {
    a = b = sqrt(n);
    while (a * b != n) {
        b++;
        a = n / b;
    }
    if((a * b) != n) {
        Logging::print(Logging::LOG_LEVEL::ERROR, "Factorization failed\n");
        std::exit(Env::finalize()); 
    }
}

template<typename Weight>
bool Tiling<Weight>::assert_tiling() {
    bool success = true;
    std::vector<int32_t> uniques(nranks);
    for(uint32_t i = 0; i < nrowgrps; i++) {
        int32_t r = tiles[i][i].rank;
        uniques[r]++;
        if(uniques[r] > 1) {
            success = false;
            break;
        }
    }
    return(success);
}

template<typename Weight>
void Tiling<Weight>::print_tiling(const std::string field) {
    Logging::print(Logging::LOG_LEVEL::INFO, "%s:\n", field.c_str());
    const uint32_t skip = 15;
    for (uint32_t i = 0; i < nrowgrps; i++) {
        for (uint32_t j = 0; j < ncolgrps; j++) {  
            auto& tile = tiles[i][j];   
            if(field.compare("rank") == 0) {
                Logging::print(Logging::LOG_LEVEL::VOID, "%d ", tile.rank);
            }
            else if(field.compare("thread") == 0) {
                Logging::print(Logging::LOG_LEVEL::VOID, "%d ", tile.thread);
            }
            else if(field.compare("nedges") == 0) {
                Logging::print(Logging::LOG_LEVEL::VOID, "%d ", tile.nedges);
            }
            else if(field.compare("height") == 0) {
                Logging::print(Logging::LOG_LEVEL::VOID, "%d ", tile.height);
            }
            else if(field.compare("width") == 0) {
                Logging::print(Logging::LOG_LEVEL::VOID, "%d ", tile.width);
            }
            if(j > skip) {
                Logging::print(Logging::LOG_LEVEL::VOID, "...");
                break;
            }
        }
        Logging::print(Logging::LOG_LEVEL::VOID, "\n");
        if(i > skip) {
            Logging::print(Logging::LOG_LEVEL::VOID, ".\n.\n.\n");
            break;
        }
    }
    Logging::print(Logging::LOG_LEVEL::VOID, "\n");
}

template<typename Weight>
void Tiling<Weight>::exchange_triples() {
    Env::barrier();
    Logging::print(Logging::LOG_LEVEL::INFO, "Tile exchange: Start exchanging tiles...\n", nranks);
    
    // Sanity check for the number of edges 
    uint64_t nedges_start_local  = 0;
    uint64_t nedges_end_local    = 0;
    uint64_t nedges_start_global = 0;
    uint64_t nedges_end_global   = 0;

    std::vector<MPI_Request> out_requests;
    std::vector<MPI_Request> in_requests;
    
    MPI_Request request;     
    
    for (uint32_t i = 0; i < nrowgrps; i++) {
        for (uint32_t j = 0; j < ncolgrps; j++) {
            auto& tile = tiles[i][j];
            auto& triples = tile.triples;
            nedges_start_local +=  (triples.empty()) ? 0 : triples.size();
        }
    }
      
    std::vector<std::vector<Triple<Weight>>> outboxes(nranks);
    for (uint32_t i = 0; i < nrowgrps; i++) {
        for (uint32_t j = 0; j < ncolgrps; j++)   {
            auto& tile = tiles[i][j];
            if(tile.rank != Env::rank) {
                auto& outbox = outboxes[tile.rank];
                outbox.insert(outbox.end(), tile.triples.begin(), tile.triples.end());
                tile.triples.clear();
                tile.triples.shrink_to_fit();
            }
        }
    }
    
    MPI_Datatype MANY_TRIPLES;
    MPI_Type_contiguous(sizeof(Triple<Weight>), MPI_BYTE, &MANY_TRIPLES);
    MPI_Type_commit(&MANY_TRIPLES);
    
    std::vector<std::vector<Triple<Weight>>> inboxes(nranks);
    std::vector<uint32_t> inbox_sizes(nranks);    
    for (uint32_t r = 0; r < nranks; r++) {
        if (r != (uint32_t) Env::rank) {
            auto& outbox = outboxes[r];
            uint32_t outbox_size = outbox.size();
            MPI_Sendrecv(&outbox_size, 1, MPI_UNSIGNED, r, 0, &inbox_sizes[r], 1, MPI_UNSIGNED,
                                                        r, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            auto& inbox = inboxes[r];
            inbox.resize(inbox_sizes[r]);
            MPI_Sendrecv(outbox.data(), outbox.size(), MANY_TRIPLES, r, 0, inbox.data(), inbox.size(), MANY_TRIPLES,
                                                                     r, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }
    }

    auto retval = MPI_Type_free(&MANY_TRIPLES);
    if(retval != MPI_SUCCESS) {
        Logging::print(Logging::LOG_LEVEL::ERROR, "Tile exchanging failed!\n");
        std::exit(Env::finalize()); 
    }
    
    // Insert exchanged triples
    uint64_t exchange_size_local = 0;
    for (uint32_t r = 0; r < nranks; r++) {
        if (r != (uint32_t) Env::rank) {
            auto& inbox = inboxes[r];
            if(not inbox.empty()) {
                insert_triples(inbox);
                delete_triples(inbox);
                exchange_size_local += inbox.size();

            }
        }
    }
    
    // Finzalize sanity check 
    for (uint32_t i = 0; i < nrowgrps; i++) {
        for (uint32_t j = 0; j < ncolgrps; j++) {
            auto& tile = tiles[i][j];
            if(tile.rank == Env::rank) {
                nedges_end_local += tile.triples.size();
            }
        }
    }    
    MPI_Allreduce(&nedges_start_local, &nedges_start_global, 1, MPI_UNSIGNED_LONG, MPI_SUM, MPI_COMM_WORLD);
    MPI_Allreduce(  &nedges_end_local,   &nedges_end_global, 1, MPI_UNSIGNED_LONG, MPI_SUM, MPI_COMM_WORLD);
    if(nedges_start_global != nedges_end_global) {
        Logging::print(Logging::LOG_LEVEL::ERROR, "Tile exchange failed\n");
        std::exit(Env::finalize()); 
    }
    
    uint64_t exchange_size_global = 0;
    MPI_Allreduce(&exchange_size_local, &exchange_size_global, 1, MPI_UNSIGNED_LONG, MPI_SUM, MPI_COMM_WORLD);
    Logging::print(Logging::LOG_LEVEL::INFO, "Tile exchange: Exchanged %lu edges.\n", exchange_size_global);
    
    Logging::print(Logging::LOG_LEVEL::INFO, "Tile exchange: Done  exchange tiles.\n");
    Env::barrier();
}


template<typename Weight>
void Tiling<Weight>::tile_load() {
    Env::barrier();
    Logging::print(Logging::LOG_LEVEL::INFO, "Tile load: Start calculating load...\n");
    std::vector<std::vector<uint64_t>> nedges_grid(nranks, std::vector<uint64_t>(rank_ntiles));
    std::vector<uint64_t> rank_nedges(nranks);
    std::vector<uint64_t> rowgrp_nedges(nrowgrps);
    std::vector<uint64_t> colgrp_nedges(ncolgrps);
    
    uint32_t k = 0;
    for(uint32_t i = 0; i < nrowgrps; i++) {
        for(uint32_t j = 0; j < ncolgrps; j++) {
            auto& tile = tiles[i][j];
            if(Env::rank == tile.rank){
                nedges_grid[Env::rank][k] = (tile.nedges) ? tile.nedges : tile.triples.size();
                k++;
            }
        }
    }
    
    for(uint32_t r = 0; r < nranks; r++) {
        if(r != (uint32_t) Env::rank) {
            auto& out_edges = nedges_grid[Env::rank];
            auto& in_edges = nedges_grid[r];
            MPI_Sendrecv(out_edges.data(), out_edges.size(), MPI_UNSIGNED_LONG, r, Env::rank, 
                          in_edges.data(),  in_edges.size(), MPI_UNSIGNED_LONG, r, r, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }
    }
    uint64_t nedges = 0;
    std::vector<uint32_t> kth(nranks);
    for(uint32_t i = 0; i < nrowgrps; i++) {
        for(uint32_t j = 0; j < ncolgrps; j++) {
            auto& tile = tiles[i][j];
            uint32_t& k = kth[tile.rank];
            uint64_t e = nedges_grid[tile.rank][k];
            tile.nedges = e;
            rank_nedges[tile.rank] += e;
            rowgrp_nedges[i] += e;
            colgrp_nedges[j] += e;
            nedges += e;
            k++;
        }
    }
    Logging::print(Logging::LOG_LEVEL::INFO, "Tile load: Done calculating load.\n");
    Env::barrier();
    /*
    Logging::print(Logging::LOG_LEVEL::INFO, "Tile load: Start calculating imbalance.\n");
    Logging::print(Logging::LOG_LEVEL::INFO, "Tile load: Total number of edges = %lu\n", nedges);
    
    tile_load_print(rank_nedges, nedges, nranks, "rank");
    tile_load_print(rowgrp_nedges, nedges, nrowgrps, "row group");
    tile_load_print(colgrp_nedges, nedges, ncolgrps, "column group");

    Logging::print(Logging::LOG_LEVEL::INFO, "Tile load: Done calculating imbalance.\n");
    Env::barrier();
    */
    
}

template<typename Weight>
void Tiling<Weight>::tile_load_print(const std::vector<uint64_t> nedges_vec, const uint64_t nedges, const uint32_t nedges_divisor, const std::string nedges_type) {
    const double imbalance_threshold = .2;
    const double balanced_ratio = (nedges) ? nedges/nedges_divisor : 0;
    double calculated_ratio = 0;
    const int32_t skip = 15;
    uint32_t count = 0;
    
    Logging::print(Logging::LOG_LEVEL::INFO, "Tile load: Balanced number of edges per %s = %lu \n", nedges_type.c_str(), (uint64_t) balanced_ratio);
    Logging::print(Logging::LOG_LEVEL::INFO, "Tile load: Imbalance ratio per %s [0-%d]: ", nedges_type.c_str(), nedges_divisor-1);
    for(uint32_t i = 0; i < nedges_divisor; i++) {
        calculated_ratio = (nedges_vec[i]) ? (double) (nedges_vec[i] / balanced_ratio) : 0;
        if(i < skip) {
            Logging::print(Logging::LOG_LEVEL::VOID, "%2.2f ", calculated_ratio);
        }
        if(fabs(calculated_ratio - 1) > imbalance_threshold) {
            count++;
        }
    }
    
    if(Env::nranks > skip) {
        Logging::print(Logging::LOG_LEVEL::VOID, "...\n");
    }
    else {
        Logging::print(Logging::LOG_LEVEL::VOID, "\n");
    }

    if(count) {
        Logging::print(Logging::LOG_LEVEL::INFO, "Tile load: Imbalance found among %d %ss are not balanced.\n", count, nedges_type.c_str());
    }
}

template<typename Weight>
void Tiling<Weight>::repartition_tiles(const std::string input_file, const INPUT_TYPE input_type) {
    Env::barrier();
    Logging::print(Logging::LOG_LEVEL::INFO, "Tile repartitioning: Start repartitioning tiles\n");
        
    MPI_Datatype MANY_TRIPLES;
    MPI_Type_contiguous(sizeof(Triple<Weight>), MPI_BYTE, &MANY_TRIPLES);
    MPI_Type_commit(&MANY_TRIPLES);
    
    MPI_Status status;   
    MPI_Request request;   
    std::vector<MPI_Request> requests;  
    std::vector<MPI_Request> send_requests;      
    std::vector<MPI_Request> recv_requests;  
    
    uint64_t balanced_nnz_per_tile = nnz/ntiles; 

    std::vector<std::vector<uint32_t>> nnz_local;
    std::vector<uint32_t> nnz_global;    

    if (tiling_type == TILING_TYPE::_1D_ROW_) {
        const RowSort<Weight> f_row;
        for (uint32_t i = 0; i < nrowgrps; i++) {
            for (uint32_t j = 0; j < ncolgrps; j++) {
                auto& tile = tiles[i][j];
                if(tile.rank == Env::rank) {
                    std::sort(tile.triples.begin(), tile.triples.end(), f_row); 
                }
            }
        }  
        
        nnz_local.resize(rank_nrowgrps);
        for(uint32_t k = 0; k < rank_nrowgrps; k++) {
            nnz_local[k].resize(tile_height);
        }
        nnz_global.resize(nrows);
        
        uint32_t k = 0;
        for (uint32_t i = 0; i < nrowgrps; i++) {
            for (uint32_t j = 0; j < ncolgrps; j++) {
                auto& tile = tiles[i][j];
                if(tile.rank == Env::rank) {
                    auto& triples = tile.triples;
                    uint32_t height = tile.height;
                    uint32_t width = tile.width;
                    bool b = false;
                    uint64_t n = 0;
                    uint32_t row = triples[0].row;
                    uint32_t col = 0;
                    for(auto triple: triples) {
                        while((row != triple.row) and (row < ((i+1) * height))) {
                            nnz_local[k][row%height] = col;
                            row++;
                            col = 0;
                        }
                        col++;
                    }
                    nnz_local[k][row%height] = col;
                    k++;
                }
            }
        }
        
        std::vector<uint32_t> offsets(ntiles);
        std::vector<uint32_t> ks(nranks);
        for (uint32_t i = 0; i < nrowgrps; i++) {
            for (uint32_t j = 0; j < ncolgrps; j++) {
                auto& tile = tiles[i][j];
                if(one_rank) {
                    auto& k = ks[0];
                    offsets[k] = tile.start_row;
                    k++;
                }
                else {
                    auto& k = ks[tile.rank];
                    offsets[(tile.rank*rank_nrowgrps)+k] = tile.start_row;
                    k++;
                }
            }
        }
                
        if (not one_rank) {   
            if(Env::rank == 0) {        
                for(uint32_t r = 1; r < nranks; r++) { 

                    for(uint32_t k = 0; k < rank_nrowgrps; k++) {
                        MPI_Irecv(nnz_global.data() + offsets[(r*rank_nrowgrps)+k], tile_height, MPI_UNSIGNED, r, r, MPI_COMM_WORLD, &request);
                        requests.push_back(request);
                    }
                }
            } 
            else {
                for(uint32_t k = 0; k < rank_nrowgrps; k++) {
                    MPI_Isend(nnz_local[k].data(), nnz_local[k].size(), MPI_UNSIGNED, 0, Env::rank, MPI_COMM_WORLD, &request); 
                    requests.push_back(request);
                }
            }

            MPI_Waitall(requests.size(), requests.data(), MPI_STATUSES_IGNORE);
            requests.clear();
            requests.shrink_to_fit();
            Env::barrier();
        }
        
        std::vector<uint32_t> partitions_start(ntiles);
        std::vector<uint32_t> partitions_end(ntiles);
        std::vector<uint32_t> partitions_nnz(ntiles);
    
        if(((not one_rank) and Env::rank == 0) or (one_rank)) {
            for(uint32_t k = 0; k < rank_nrowgrps; k++) {
                if(one_rank) {
                    std::copy(nnz_local[k].begin(), nnz_local[k].end(), nnz_global.begin() + offsets[k]);
                }
                else {
                    std::copy(nnz_local[k].begin(), nnz_local[k].end(), nnz_global.begin() + offsets[(Env::rank*rank_nrowgrps)+k]);
                }
            }
            uint64_t global_sum_nnz = std::accumulate(nnz_global.begin(), nnz_global.end(), 0);
            if(global_sum_nnz != nnz) {
                Logging::print(Logging::LOG_LEVEL::ERROR, "Repartitioning error\n");
                std::exit(Env::finalize()); 
            }
            
            uint64_t m = 0;
            uint32_t n = 0;
            uint32_t start = 0;
            uint32_t end = 0;
            uint32_t t = 0;
            for(uint32_t i = 0; i < nrows; i++) {
                n += nnz_global[i];
                if(((int64_t) (balanced_nnz_per_tile - n) < 0) or ((i+1) == nrows)) {
                    partitions_start[t] = (t == 0)         ? 0         : partitions_end[t-1];
                    partitions_end[t]   = (t == ntiles-1)  ? nrows     : i;
                    partitions_nnz[t]   = (t == ntiles-1)  ? (nnz - m) : (n - nnz_global[i]);
                    i                   = (t == ntiles-1)  ? nrows     : (i - 1);
                    m += partitions_nnz[t];
                    n = 0;
                    t++;
                }
            }
            global_sum_nnz = std::accumulate(partitions_nnz.begin(), partitions_nnz.end(), 0);
            if(global_sum_nnz != nnz) {
                Logging::print(Logging::LOG_LEVEL::ERROR, "Repartitioning error\n");
                std::exit(Env::finalize()); 
            }
            uint32_t global_sum_nrows = 0;
            for(uint32_t t = 0; t < ntiles; t++) {
                global_sum_nrows += partitions_end[t] - partitions_start[t];
            }
            if(global_sum_nrows != nrows) {
                Logging::print(Logging::LOG_LEVEL::ERROR, "Repartitioning error\n");
                std::exit(Env::finalize()); 
            }        
        }
    
        for(uint32_t k = 0; k < rank_nrowgrps; k++) {
            nnz_local[k].clear();
            nnz_local[k].shrink_to_fit();
        }
        nnz_local.clear();
        nnz_local.shrink_to_fit();

        if(not one_rank) {
            std::vector<uint32_t> partitions(ntiles*3);
            Env::barrier();
            if(Env::rank == 0) {
                for(uint32_t t = 0; t < ntiles; t++) {
                    partitions[(t*3)] = partitions_start[t];
                    partitions[(t*3)+1] = partitions_end[t];
                    partitions[(t*3)+2] = partitions_nnz[t];
                }
                
                for(uint32_t r = 1; r < nranks; r++) {
                    MPI_Isend(partitions.data(), partitions.size(), MPI_UNSIGNED, r, r, MPI_COMM_WORLD, &request); 
                    requests.push_back(request);
                }     
                MPI_Waitall(requests.size(), requests.data(), MPI_STATUSES_IGNORE);
                requests.clear();
                requests.shrink_to_fit();
            }
            else {
                MPI_Recv(partitions.data(), partitions.size(), MPI_UNSIGNED, 0, Env::rank, MPI_COMM_WORLD, &status);
                for(uint32_t t = 0; t < ntiles; t++) {
                    partitions_start[t] = partitions[(t*3)];
                    partitions_end[t] = partitions[(t*3)+1];
                    partitions_nnz[t] = partitions[(t*3)+2];
                }
            }
            Env::barrier();
        }
        
        for (uint32_t i = 0; i < nrowgrps; i++) {
            for (uint32_t j = 0; j < ncolgrps; j++) {
                auto& tile = tiles[i][j];
                tile.start_row = partitions_start[i];
                tile.end_row = partitions_end[i];
                tile.height = tile.end_row - tile.start_row;
                tile.nedges = partitions_nnz[i];
                if(tile.rank == Env::rank) {
                    auto& triples = tile.triples;
                    triples.clear();
                    triples.shrink_to_fit();
                }
            }
        }
        
        for (uint32_t i = 0; i < nrowgrps; i++) {
            bounds[i] = tiles[i][0].end_row;
        }
        
        std::vector<struct Triple<Weight>> triples;
        if(INPUT_TYPE::_TEXT_ == input_type) {
            triples = IO::text_file_read<Weight>(input_file, one_rank);
        }
        else {
            triples = IO::binary_file_read<Weight>(input_file, one_rank);
        }
        
        insert_triples(triples);
        delete_triples(triples);

        if(not one_rank) {
            exchange_triples();
        }
        
        for (uint32_t i = 0; i < nrowgrps; i++) {
            for (uint32_t j = 0; j < ncolgrps; j++) {
                auto& tile = tiles[i][j];
                if(tile.rank == Env::rank) {
                    auto& triples = tile.triples;
                    if(tile.nedges != triples.size()) {
                        Logging::print(Logging::LOG_LEVEL::ERROR, "Repartitioning error\n");
                        std::exit(Env::finalize()); 
                    }
                }
                
            }
        }
    }
    else if (tiling_type == TILING_TYPE::_1D_COL_) {
        
        const ColSort<Weight> f_col;        
        for (uint32_t i = 0; i < nrowgrps; i++) {
            for (uint32_t j = 0; j < ncolgrps; j++) {
                auto& tile = tiles[i][j];
                if(tile.rank == Env::rank) {
                    std::sort(tile.triples.begin(), tile.triples.end(), f_col); 
                }
            }
        }  
        
        nnz_local.resize(rank_ncolgrps);
        for(uint32_t k = 0; k < rank_ncolgrps; k++) {
            nnz_local[k].resize(tile_width);
        }
        nnz_global.resize(ncols);
        
        uint32_t k = 0;
        for (uint32_t i = 0; i < nrowgrps; i++) {
            for (uint32_t j = 0; j < ncolgrps; j++) {
                auto& tile = tiles[i][j];
                if(tile.rank == Env::rank) {
                    auto& triples = tile.triples;
                    uint32_t height = tile.height;
                    uint32_t width = tile.width;
                    bool b = false;
                    uint64_t n = 0;
                    uint32_t row = 0;
                    uint32_t col = triples[0].col;
                    for(auto triple: triples) {
                        while((col != triple.col) and (col < ((j+1) * width))) {
                            nnz_local[k][col%width] = row;
                            col++;
                            row = 0;
                        }
                        row++;
                    }
                    nnz_local[k][col%width] = row;                        
                    k++;
                }
            }
        }
        
        std::vector<uint32_t> offsets(ntiles);
        std::vector<uint32_t> ks(nranks);
        for (uint32_t i = 0; i < nrowgrps; i++) {
            for (uint32_t j = 0; j < ncolgrps; j++) {
                auto& tile = tiles[i][j];
                if(one_rank) {
                    auto& k = ks[0];
                    offsets[k] = tile.start_col;
                    k++;
                }
                else {
                    auto& k = ks[tile.rank];
                    offsets[(tile.rank*rank_ncolgrps)+k] = tile.start_col;
                    k++;
                }
            }
        }
        
        if (not one_rank) {
            if(Env::rank == 0) {        
                for(uint32_t r = 1; r < nranks; r++) { 
                    for(uint32_t k = 0; k < rank_ncolgrps; k++) {
                        MPI_Irecv(nnz_global.data() + offsets[(r*rank_ncolgrps)+k], tile_width, MPI_UNSIGNED, r, r, MPI_COMM_WORLD, &request);
                        requests.push_back(request);
                    }
                }
            } 
            else {
                for(uint32_t k = 0; k < rank_ncolgrps; k++) {
                    MPI_Isend(nnz_local[k].data(), nnz_local[k].size(), MPI_UNSIGNED, 0, Env::rank, MPI_COMM_WORLD, &request); 
                    requests.push_back(request);
                }
            }

            MPI_Waitall(requests.size(), requests.data(), MPI_STATUSES_IGNORE);
            requests.clear();
            requests.shrink_to_fit();
            Env::barrier();    
        }
        

        std::vector<uint32_t> partitions_start(ntiles);
        std::vector<uint32_t> partitions_end(ntiles);
        std::vector<uint32_t> partitions_nnz(ntiles);
    
        if(((not one_rank) and Env::rank == 0) or (one_rank)) {
            for(uint32_t k = 0; k < rank_ncolgrps; k++) {
                if(one_rank) {
                    std::copy(nnz_local[k].begin(), nnz_local[k].end(), nnz_global.begin() + offsets[k]);
                }
                else {
                    std::copy(nnz_local[k].begin(), nnz_local[k].end(), nnz_global.begin() + offsets[(Env::rank*rank_ncolgrps)+k]);
                }
            }
            uint64_t global_sum_nnz = std::accumulate(nnz_global.begin(), nnz_global.end(), 0);
            if(global_sum_nnz != nnz) {
                Logging::print(Logging::LOG_LEVEL::ERROR, "Repartitioning error\n");
                std::exit(Env::finalize()); 
            }
            
            uint64_t m = 0;
            uint32_t n = 0;
            uint32_t start = 0;
            uint32_t end = 0;
            uint32_t t = 0;
            for(uint32_t j = 0; j < ncols; j++) {
                n += nnz_global[j];
                if(((int64_t) (balanced_nnz_per_tile - n) < 0) or ((j+1) == ncols)) {
                    partitions_start[t] = (t == 0)         ? 0         : partitions_end[t-1];
                    partitions_end[t]   = (t == ntiles-1)  ? ncols     : j;
                    partitions_nnz[t]   = (t == ntiles-1)  ? (nnz - m) : (n - nnz_global[j]);
                    j                   = (t == ntiles-1)  ? ncols     : (j - 1);
                    m += partitions_nnz[t];
                    n = 0;
                    t++;
                }
            }
            global_sum_nnz = std::accumulate(partitions_nnz.begin(), partitions_nnz.end(), 0);
            if(global_sum_nnz != nnz) {
                Logging::print(Logging::LOG_LEVEL::ERROR, "Repartitioning error\n");
                std::exit(Env::finalize()); 
            }
            uint32_t global_sum_ncols = 0;
            for(uint32_t t = 0; t < ntiles; t++) {
                global_sum_ncols += partitions_end[t] - partitions_start[t];
            }
            if(global_sum_ncols != ncols) {
                Logging::print(Logging::LOG_LEVEL::ERROR, "Repartitioning error\n");
                std::exit(Env::finalize()); 
            }         
        }
    
        for(uint32_t k = 0; k < rank_ncolgrps; k++) {
            nnz_local[k].clear();
            nnz_local[k].shrink_to_fit();
        }
        nnz_local.clear();
        nnz_local.shrink_to_fit();
        
        if(not one_rank) {
            std::vector<uint32_t> partitions(ntiles*3);
            Env::barrier();
            if(Env::rank == 0) {
                for(uint32_t t = 0; t < ntiles; t++) {
                    partitions[(t*3)] = partitions_start[t];
                    partitions[(t*3)+1] = partitions_end[t];
                    partitions[(t*3)+2] = partitions_nnz[t];
                }
                
                for(uint32_t r = 1; r < nranks; r++) {
                    MPI_Isend(partitions.data(), partitions.size(), MPI_UNSIGNED, r, r, MPI_COMM_WORLD, &request); 
                    requests.push_back(request);
                }     
                MPI_Waitall(requests.size(), requests.data(), MPI_STATUSES_IGNORE);
                requests.clear();
                requests.shrink_to_fit();
            }
            else {
                MPI_Recv(partitions.data(), partitions.size(), MPI_UNSIGNED, 0, Env::rank, MPI_COMM_WORLD, &status);
                for(uint32_t t = 0; t < ntiles; t++) {
                    partitions_start[t] = partitions[(t*3)];
                    partitions_end[t] = partitions[(t*3)+1];
                    partitions_nnz[t] = partitions[(t*3)+2];
                }
            }
            Env::barrier();
        }
        
        for (uint32_t i = 0; i < nrowgrps; i++) {
            for (uint32_t j = 0; j < ncolgrps; j++) {
                auto& tile = tiles[i][j];
                tile.start_col = partitions_start[j];
                tile.end_col = partitions_end[j];
                tile.width = tile.end_col - tile.start_col;
                tile.nedges = partitions_nnz[j];
                if(tile.rank == Env::rank) {
                    auto& triples = tile.triples;
                    triples.clear();
                    triples.shrink_to_fit();
                }
            }
        }
        
        for (uint32_t j = 0; j < ncolgrps; j++) {
            bounds[j] = tiles[0][j].end_col;
        }
        
        std::vector<struct Triple<Weight>> triples;
        if(INPUT_TYPE::_TEXT_ == input_type) {
            triples = IO::text_file_read<Weight>(input_file, one_rank);
        }
        else {
            triples = IO::binary_file_read<Weight>(input_file, one_rank);
        }
        
        insert_triples(triples);
        delete_triples(triples);
        
        if(not one_rank) {
            exchange_triples();
        }
        
        for (uint32_t i = 0; i < nrowgrps; i++) {
            for (uint32_t j = 0; j < ncolgrps; j++) {
                auto& tile = tiles[i][j];
                if(tile.rank == Env::rank) {
                    auto& triples = tile.triples;
                    if(tile.nedges != triples.size()) {
                        Logging::print(Logging::LOG_LEVEL::ERROR, "Repartitioning error\n");
                        std::exit(Env::finalize()); 
                    }
                }
            }
        }
    }

    auto retval = MPI_Type_free(&MANY_TRIPLES);
    if(retval != MPI_SUCCESS) {
        Logging::print(Logging::LOG_LEVEL::ERROR, "Tile repartitioning failed!\n");
        std::exit(Env::finalize()); 
    }

    Logging::print(Logging::LOG_LEVEL::INFO, "Tile repartitioning: Done repartitioning tiles.\n");
    Env::barrier();
}

template<typename Weight>
void Tiling<Weight>::insert_triples(std::vector<struct Triple<Weight>>& triples){
    if (tiling_type == TILING_TYPE::_1D_ROW_) {
        for(auto triple: triples) {
            for(uint32_t i = 0; i < nrowgrps; i++) {
                if(triple.row < bounds[i]) {
                    tiles[i][0].triples.push_back(triple);
                    break;
                }
            }
        }
    }
    else if (tiling_type == TILING_TYPE::_1D_COL_) {
        for(auto triple: triples) {
            for(uint32_t j = 0; j < ncolgrps; j++) {
                if(triple.col < bounds[j]) {
                    tiles[0][j].triples.push_back(triple);
                    break;
                }
            }
        }
    }
}

template<typename Weight>
void Tiling<Weight>::delete_triples(std::vector<struct Triple<Weight>>& triples){
    triples.clear();
    triples.shrink_to_fit();
}

template<typename Weight>
uint32_t Tiling<Weight>::get_tile_info(const std::string field, const int32_t tid) {
    for (uint32_t i = 0; i < nrowgrps; i++) {
        for (uint32_t j = 0; j < ncolgrps; j++) {
            auto& tile = tiles[i][j];
            if((tile.rank == Env::rank) and (tile.thread == tid)) {
                if(field.compare("start_row") == 0) {
                    return(tile.start_row);
                }
                else if(field.compare("height") == 0) {
                    return(tile.height);
                }
            }
        }
    }
    Logging::print(Logging::LOG_LEVEL::WARN, "Tile info: Something is wrong.\n");
    return(0);
}

template<typename Weight>
void Tiling<Weight>::set_tile_info(const std::vector<std::vector<struct Tile<Weight>>> other_tiles) {
    for (uint32_t i = 0; i < nrowgrps; i++) {
        for (uint32_t j = 0; j < ncolgrps; j++) {
            auto& tile = tiles[i][j];
            auto other = other_tiles[i][j];
            tile.start_row = other.start_row;
            tile.end_row = other.end_row;
            tile.start_col = other.start_col;
            tile.end_col = other.end_col;
            tile.height = other.height;
            tile.width = other.width;
        }
    }
    print_tiling("height");
}

template<typename Weight>
void Tiling<Weight>::compress_triples(){//(const REFINE_TYPE refine_type) {
    Env::barrier();
    Logging::print(Logging::LOG_LEVEL::INFO, "Tile compression: Start compressing tile using CSC\n");

    for (uint32_t i = 0; i < nrowgrps; i++) {
        for (uint32_t j = 0; j < ncolgrps; j++) {
            auto& tile = tiles[i][j];
            if(tile.rank == Env::rank) {
                tile.compress(one_rank);
            }
        }
    }    

    Logging::print(Logging::LOG_LEVEL::INFO, "Tile compression: Done compressing tiles.\n");
    Env::barrier();      
}

template<typename Weight>
void Tiling<Weight>::test1() {
    Env::barrier();
    printf("Rank=%d\n", Env::rank);
    //std::vector<int32_t> idle_ranks;
    //Env::create_mpi_asynch_shared_mem<int32_t>(&Env::idle_ranks, Env::nranks+1, &Env::window);
    //Env::barrier();
    //char* has_changed;
    //int* num_idle_ranks;
    
    //if (Env::rank == 0) {
        //window_size = Env::nranks * sizeof(int32_t);
    
    /*
    std::vector<int32_t> idle_ranks;
    uint32_t window_size = (Env::nranks + 1) * sizeof(int32_t);
    MPI_Alloc_mem(window_size, MPI_INFO_NULL, &idle_ranks);
    memset(idle_ranks.data(), -1, window_size);
    //std::fill(idle_ranks.begin(), idle_ranks.end(), -1);

    idle_ranks[Env::nranks] = 0;
        //addr = 1; // initialised to 1 since MPI_Fetch_and_op returns value *before* increment
    //}
    Env::barrier();
    MPI_Win window;
    MPI_Win_create(idle_ranks.data(), window_size, sizeof(int32_t), MPI_INFO_NULL, MPI_COMM_WORLD, &window);
    */
    /*    
    int counter;
    int idle_status = 1;
    int idle_rank = Env::rank+1;
    for(int i = 1; i < Env::nranks; i++) {
        int target_rank = (Env::rank+i)%Env::nranks;
        int target_disp = Env::rank;
        MPI_Win_lock(MPI_LOCK_EXCLUSIVE, target_rank, 0, Env::window);
        MPI_Fetch_and_op(&idle_status, &counter, MPI_INT, target_rank, Env::nranks, MPI_SUM, Env::window);
        MPI_Fetch_and_op(&idle_rank, &counter, MPI_INT, target_rank, target_disp, MPI_REPLACE, Env::window);
        MPI_Win_unlock(target_rank, Env::window);    
    }
    
    Env::barrier();  
    if (Env::rank == 0) {
        printf("%d: ", Env::rank);
        //for(auto i: Env::idle_ranks) {printf("%d ", i);}
        for(int i = 0;i < Env::nranks+1; i++) {
            printf("%d ", *(Env::idle_ranks + i));
            //printf("%d ", Env::idle_ranks[i]);
            
        }
        printf("\n");
    }
    Env::barrier();  
    if (Env::rank == 1) {
        printf("%d: ", Env::rank);
        for(int i =0;i < Env::nranks+1; i++) {
            printf("%d ", *(Env::idle_ranks + i));
            //printf("%d ", Env::idle_ranks[i]);
        }
        printf("\n");
    }
    Env::barrier();  
    if (Env::rank == 2) {
        printf("%d: ", Env::rank);
        for(int i =0;i < Env::nranks+1; i++) {
            printf("%d ", *(Env::idle_ranks + i));
            //printf("%d ", Env::idle_ranks[i]);
        }
        printf("\n");
    }
    Env::barrier();  
    if (Env::rank == 3) {
        printf("%d: ", Env::rank);
        for(int i =0;i < Env::nranks+1; i++) {
            printf("%d ", *(Env::idle_ranks + i));
            //printf("%d ", Env::idle_ranks[i]);
        }
        printf("\n");
    }
    */
    //Env::barrier();  
    //Env::destroy_mpi_asynch_shared_mem(&Env::window);
    //MPI_Win_free(&window);   
    
    //if (Env::rank == 0) {
        //idle_ranks.clear();
        //idle_ranks.shrink_to_fit();
        //MPI_Free_mem(idle_ranks);
    //}
    
    Env::barrier();    
    //printf("%d %d\n", Env::rank, counter);    

    /*
    int *addr = 0;
    int winSz = 0;
    if (Env::rank == 0) {
        winSz = sizeof(int);
        MPI_Alloc_mem(winSz, MPI_INFO_NULL, &addr);
        *addr = 1; // initialised to 1 since MPI_Fetch_and_op returns value *before* increment
    }
    MPI_Win win;
    MPI_Win_create(addr, winSz, sizeof(int), MPI_INFO_NULL, MPI_COMM_WORLD, &win);
    
    int counter;
    one = 1;
    MPI_Win_lock(MPI_LOCK_EXCLUSIVE, 0, 0, win);
    MPI_Fetch_and_op(&one, &counter, MPI_INT, 0, 0, MPI_SUM, win);
    MPI_Win_unlock(0, win);

    // checking the value of the counter and printing by last in time process
    if (counter == Env::nranks) {
        std::cout << "Process #" << Env::rank << " did the last update" << std::endl;
    }

    // cleaning up
    MPI_Win_free(&win);
    if (Env::rank == 0) {
        MPI_Free_mem(addr);
    }    
    */
    
    /*
    if(Env::rank == 0) {
        for(int32_t i = 1; i < Env::nranks; i++) {
            int32_t r = (Env::rank + i) % Env::nranks;
            printf("%d %d\n", i, r);
        }
    }
    Env::barrier();
    if(Env::rank == 1) {
        for(int32_t i = 1; i < Env::nranks; i++) {
            int32_t r = (Env::rank + i) % Env::nranks;
            printf("%d %d\n", i, r);
        }
    }
    Env::barrier();
    if(Env::rank == 2) {
        for(int32_t i = 1; i < Env::nranks; i++) {
            int32_t r = (Env::rank + i) % Env::nranks;
            printf("%d %d\n", i, r);
        }
    }
    Env::barrier();
    if(Env::rank == 3) {
        for(int32_t i = 1; i < Env::nranks; i++) {
            int32_t r = (Env::rank + i) % Env::nranks;
            printf("%d %d\n", i, r);
        }
    }
    */
    Env::barrier();
    std::exit(0);
    
}

template<typename Weight>
void Tiling<Weight>::test() {
    
    MPI_Request request;
    std::vector<MPI_Request> requests;
    MPI_Datatype WEIGHT_TYPE = MPI_Types::get_mpi_data_type<Weight>();
    
    if(Env::rank == 0) {
        auto& tile = tiles[0][0];
        std::shared_ptr<struct CSC<Weight>> CSC = tile.spmat;
        uint32_t nlocals = 2;
        uint32_t nremotes = 3;
        std::vector<std::shared_ptr<struct CSC<Weight>>> CSCs;
        CSC->split(CSCs, nlocals, nremotes);

        
        uint32_t nparts = 1 + nremotes;
        tile.subtiles.resize(nparts);
        uint32_t my_start_row = tile.start_row;
        for(uint32_t k = 0; k < nparts; k++) {
            struct Tile<Weight>& subtile = tile.subtiles[k];
            std::shared_ptr<struct CSC<Weight>>& subcsc = CSCs[k];
            
            //subtile.rank = ?;
            //subtile.thread = ?;
            subtile.nedges = subcsc->nnz;
            subtile.start_row = my_start_row;
            subtile.end_row = my_start_row + subcsc->nrows;
            subtile.start_col = 0;
            subtile.end_col = subcsc->ncols;
            subtile.height = subcsc->nrows;
            subtile.width = subcsc->ncols;
            my_start_row += subcsc->nrows;
            
            //subtile.spmat  = std::move(subcsc);
            
            
            
            
            
            //printf("%d:\n",k);
            //printf("%d %lu\n", subtile.rank, subtile.nedges);
            //printf("[%d %d] [%d %d][%d %d]\n", subtile.start_row, subtile.end_row, subtile.start_col, subtile.end_col, subtile.height, subtile.width);
        }
        
        //printf("DONE %lu\n", CSCs.size());
        //for(auto subtile: tile.subtiles) {
        //    printf("%d\n", subtile.height);
        //}
        
        
        //tile.subtiles[0].spmat  = std::move(CSCs[0]);
        //printf("%d: sending to 1\n", Env::rank);
        
        //std::vector<uint32_t> csc_metadata(5);
        //csc_metadata.resize(5);
        for(uint32_t k = 0; k < nparts; k++) {
            printf("%d %d\n", k, Env::rank);
            struct Tile<Weight>& subtile = tile.subtiles[k];
            
            uint32_t layer = 0;
            uint32_t local_rowgroup = 0;
            std::vector<uint32_t> csc_metadata = {layer, local_rowgroup, (uint32_t)subtile.nedges, subtile.start_row, subtile.height, subtile.width};
            MPI_Send(csc_metadata.data(), csc_metadata.size(), MPI_UNSIGNED, 1, k, MPI_COMM_WORLD);
        
        }
        

        for(uint32_t k = 1; k < nparts; k++) {
            std::shared_ptr<struct CSC<Weight>>& subcsc = CSCs[k];
            uint32_t* JA = subcsc->JA_blk->ptr;
            uint32_t* IA = subcsc->IA_blk->ptr;
            Weight*    A = subcsc->A_blk->ptr;
            
            MPI_Isend(JA, subcsc->JA_blk->nitems, MPI_UNSIGNED, 1, (k*3)+0, MPI_COMM_WORLD, &request);
            requests.push_back(request);
            MPI_Isend(IA, subcsc->IA_blk->nitems, MPI_UNSIGNED, 1, (k*3)+1, MPI_COMM_WORLD, &request);
            requests.push_back(request);
            MPI_Isend(A, subcsc->A_blk->nitems,    WEIGHT_TYPE, 1, (k*3)+2, MPI_COMM_WORLD, &request);
            requests.push_back(request);
        }
        
        MPI_Waitall(requests.size(), requests.data(), MPI_STATUSES_IGNORE);
        requests.clear();
        requests.shrink_to_fit();
        
        CSCs.clear();
        CSCs.shrink_to_fit();
    
    } else {
        
        printf("%d: receiving from 0\n", Env::rank);
        uint32_t nremotes = 3;
        uint32_t nparts = 1 + nremotes;
        //MPI_Status status;
        //MPI_Request request;
        //std::vector<MPI_Request> requests;
        std::vector<std::vector<uint32_t>> cscs_metadata(nparts);
        for(uint32_t k = 0; k < cscs_metadata.size(); k++) {
            cscs_metadata[k].resize(6);
        }
        
        for(uint32_t k = 0; k < nparts; k++) {
            printf("%d: %d\n", k, Env::rank);
            //std::vector<uint32_t> csc_metadata(6);
            auto& csc_metadata = cscs_metadata[k];
            MPI_Irecv(csc_metadata.data(), csc_metadata.size(), MPI_UNSIGNED, 0, k, MPI_COMM_WORLD, &request);
            requests.push_back(request);
            //printf("%d: %d %d %d %d %d\n", k, csc_metadata[0], csc_metadata[1], csc_metadata[2], csc_metadata[3], csc_metadata[4]);
        }
        MPI_Waitall(requests.size(), requests.data(), MPI_STATUSES_IGNORE);
        requests.clear();
        requests.shrink_to_fit();
        uint32_t layer = cscs_metadata[0][0];
        uint32_t remote_rowgrop = cscs_metadata[0][1];
        auto& tile = tiles[remote_rowgrop][0];
        tile.subtiles.resize(nparts);
        //uint32_t my_start_row = csc_metadatas;
        for(uint32_t k = 0; k < nparts; k++) {
            auto& csc_metadata = cscs_metadata[k];
            uint64_t csc_nedges = (uint64_t) csc_metadata[2];
            uint32_t csc_start_row = csc_metadata[3];
            uint32_t csc_height = csc_metadata[4];
            uint32_t csc_width = csc_metadata[5];
            
            struct Tile<Weight>& subtile = tile.subtiles[k];
            subtile.nedges = csc_nedges;
            subtile.start_row = csc_start_row;
            subtile.end_row = csc_start_row + csc_height;
            subtile.start_col = 0;
            subtile.end_col = csc_width;
            subtile.height = csc_height;
            subtile.width = csc_width;
        }
        
        for(uint32_t k = 1; k < nparts; k++) {
            struct Tile<Weight>& subtile = tile.subtiles[k];
            subtile.spmat = std::make_shared<struct CSC<Weight>>(subtile.nedges, subtile.height, subtile.width);
            
            uint32_t* JA = subtile.spmat->JA_blk->ptr;
            uint32_t* IA = subtile.spmat->IA_blk->ptr;
            Weight*    A = subtile.spmat->A_blk->ptr;
            MPI_Irecv(JA, subtile.spmat->JA_blk->nitems, MPI_UNSIGNED, 0, (k*3)+0, MPI_COMM_WORLD, &request);
            requests.push_back(request);
            MPI_Irecv(IA, subtile.spmat->IA_blk->nitems, MPI_UNSIGNED, 0, (k*3)+1, MPI_COMM_WORLD, &request);
            requests.push_back(request);
            MPI_Irecv(A, subtile.spmat->A_blk->nitems,    WEIGHT_TYPE, 0, (k*3)+2, MPI_COMM_WORLD, &request);
            requests.push_back(request);
        }
        
        MPI_Waitall(requests.size(), requests.data(), MPI_STATUSES_IGNORE);
        requests.clear();
        requests.shrink_to_fit();
        
        
        for(uint32_t k = 0; k < cscs_metadata.size(); k++) {
            cscs_metadata[k].clear();
            cscs_metadata[k].shrink_to_fit();
        }
        cscs_metadata.clear();
        cscs_metadata.shrink_to_fit();
        
        /*
        uint64_t checkcount = 0;
        double checksum = 0;
        for(uint32_t k = 1; k < nparts; k++) {
            struct Tile<Weight>& subtile = tile.subtiles[k];
            

        uint64_t checkcount1 = 0;
        uint32_t ncols1 = subtile.spmat->ncols;
        uint32_t*   IA1 = subtile.spmat->IA_blk->ptr;
        uint32_t*   JA1 = subtile.spmat->JA_blk->ptr;
        double*      A1 = subtile.spmat->A_blk->ptr;
        for(uint32_t j = 0; j < ncols1; j++) {
            for(uint32_t i = JA1[j]; i < JA1[j+1]; i++) {
                checkcount1++;
                checksum += A1[i];
            }
        }
        printf("%lu\n", checkcount1);
        checkcount += checkcount1;
    }    

    printf("%f %lu\nn", checksum, checkcount);
        */
        
        //
        //csc_metadata.clear();
        //csc_metadata.shrink_to_fit();
        
    }
    /*
    //Env::barrier();
    if(Env::rank == 0) {
        uint32_t value = 10;
        MPI_Send(&value, 1, MPI_UNSIGNED, 1, 0, MPI_COMM_WORLD);
    }
    else {
        MPI_Status status;
        uint32_t value = 0;
        MPI_Recv(&value, 1, MPI_UNSIGNED, 0, 0, MPI_COMM_WORLD, &status);
        printf("value = %d\n", value);
    }
    */
    
    
    /*
    for(i
        subtile.rank = 
        subtile.nedges
    }
    
    
            int32_t rank;
        int32_t thread;
        uint64_t nedges = 0;
        uint32_t start_row = 0;
        uint32_t end_row = 0;
        uint32_t start_col = 0;
        uint32_t end_col = 0;
        uint32_t height = 0;
        uint32_t width = 0;
    */
    Env::barrier();
    std::exit(0);
    
    /*
    const uint64_t  nnz   = CSC->nnz;
    const uint32_t  nrows = CSC->nrows;
    const uint32_t  ncols = CSC->ncols;
    const uint32_t*    IA = CSC->IA_blk->ptr;
    const uint32_t*    JA = CSC->JA_blk->ptr;
    const Weight*       A = CSC->A_blk->ptr;
    
    uint64_t balanced_nnz = CSC->nnz/2;
    
    printf("%d %lu %d %lu\n", Env::rank, CSC->nnz, CSC->nrows, balanced_nnz);
    
    std::vector<uint32_t> rows(nrows);
    for(uint32_t j = 0; j < ncols; j++) {
        for(uint32_t i = JA[j]; i < JA[j+1]; i++) {
            rows[IA[i]]++;
        }
    }

    
    
    uint32_t i = 0;
    uint64_t new_nnz1 = 0;
    while(new_nnz1 < balanced_nnz) {
        new_nnz1 += rows[i];
        i++;
    }
    uint32_t nrows1 = i;
    
    //uint64_t s = 0;
    //for(i = 0; i < row; i++) {
      //  s += rows[i];
    //}
    //printf("%d <%lu %lu %lu>\n", row, max_nnz, balanced_nnz, s);
    
    
    uint32_t new_height1 = nrows1;
    uint32_t new_height2 = tile.height - nrows1;
    uint32_t new_width = tile.width;
    uint64_t new_nnz2 = nnz - new_nnz1; 
    
    printf("[%d %d %d] [%lu %lu %lu]\n", new_height1, new_height2, new_height1 + new_height2, new_nnz1, new_nnz2, new_nnz1 + new_nnz2);
    std::shared_ptr<struct CSC<Weight>> CSC1 = std::make_shared<struct CSC<Weight>>(new_nnz1, new_height1, new_width);
    std::shared_ptr<struct CSC<Weight>> CSC2 = std::make_unique<struct CSC<Weight>>(new_nnz2, new_height2, new_width);
    
    uint32_t* IA1 = CSC1->IA_blk->ptr;
    uint32_t* JA1 = CSC1->JA_blk->ptr;
    Weight*    A1 = CSC1->A_blk->ptr;
    
    uint32_t* IA2 = CSC2->IA_blk->ptr;
    uint32_t* JA2 = CSC2->JA_blk->ptr;
    Weight*    A2 = CSC2->A_blk->ptr;
    
    
    //uint32_t i1 = 0;
    //uint32_t i2 = 0;
    //uint32_t j1 = 1;
    //uint32_t j2 = 1;
    JA1[0] = 0;
    JA2[0] = 0;
    for(uint32_t j = 0; j < ncols; j++) {
        //JA1[j+1] = 
        uint32_t& k1 = JA1[j+1];
        k1 = JA1[j];

        uint32_t& k2 = JA2[j+1];
        k2 = JA2[j];
        //uint32_t j1_old = JA1[j+1];
        //uint32_t j2_old = JA2[j+1];
        //if(JA[j+1] - JA[j]) {
        for(uint32_t i = JA[j]; i < JA[j+1]; i++) {
            if(IA[i] < new_height1) {
                //JA1[j+1]++
                IA1[k1] = IA[i];
                A1[k1] = A[i];
                k1++;
            }
            else {
                //JA1[j+1]++;
                IA2[k2] = IA[i] - new_height1;
                A2[k2] = A[i];
                k2++;
            }
        }
    }
    
    double checksum1   = 0;
    uint64_t checkcount1 = 0;    
    double checksum2   = 0;
    uint64_t checkcount2 = 0;    
    for(uint32_t j = 0; j < ncols; j++) {
        for(uint32_t i = JA1[j]; i < JA1[j+1]; i++) {
            checksum1 += A1[i];
            checkcount1++;
        }
    }
    
    for(uint32_t j = 0; j < ncols; j++) {
        for(uint32_t i = JA2[j]; i < JA2[j+1]; i++) {
            checksum2 += A2[i];
            checkcount2++;
        }
    }
    
    printf("[%f %lu %lu] [%f %lu %lu]\n", checksum1, checkcount1, new_nnz1, checksum2, checkcount2, new_nnz2);
    */
    

        
        //else {
        //    JA1[j+1] = JA[j];
        //    JA2[j+1] = JA[j];
        //}
    //}
    


    
    
    
    
    
    
    
}

#endif