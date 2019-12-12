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
               const TILING_TYPE tiling_type_, const COMPRESSED_FORMAT compression_type, const REFINE_TYPE refine_type = REFINE_TYPE::_REFINE_NONE_);

        Tiling(const uint32_t ntiles_, const uint32_t nrowgrps_, const uint32_t ncolgrps_, const uint32_t nranks_, 
               const uint64_t nnz_, const uint32_t nrows_, const uint32_t ncols_, 
               const TILING_TYPE tiling_type_, const COMPRESSED_FORMAT compression_type, const REFINE_TYPE refine_type = REFINE_TYPE::_REFINE_NONE_);               
                
        Tiling(const uint32_t ntiles_, const uint32_t nrowgrps_, const uint32_t ncolgrps_, const uint32_t nranks_,
               const uint32_t rank_nthreads_, const uint32_t nthreads_, 
               const uint64_t nnz_, const uint32_t nrows_, const uint32_t ncols_,
               const TILING_TYPE tiling_type_, const COMPRESSED_FORMAT compression_type, const REFINE_TYPE refine_type = REFINE_TYPE::_REFINE_NONE_);
        
        Tiling(const uint32_t ntiles_, const uint32_t nrowgrps_, const uint32_t ncolgrps_, 
               const uint32_t nranks_, const uint32_t rank_nthreads_, const uint32_t nthreads_,
               const uint64_t nnz_, const uint32_t nrows_, const uint32_t ncols_, 
               const std::string input_file, const INPUT_TYPE input_type, 
               const TILING_TYPE tiling_type_, const COMPRESSED_FORMAT compression_type, const REFINE_TYPE refine_type = REFINE_TYPE::_REFINE_NONE_);

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
        bool one_rank = false;
        void set_threads_indices();

    private:
        void integer_factorize(const uint32_t n, uint32_t& a, uint32_t& b);
        void populate_tiling();
        void print_tiling(const std::string field);
        bool assert_tiling();
        void insert_triple(const struct Triple<Weight> triple);
        void tile_exchange();
        void tile_load();
        void sort_tile(COMPRESSED_FORMAT compression_type);
        void repartition_tiles(COMPRESSED_FORMAT compression_type);
        void tile_load_print(const std::vector<uint64_t> nedges_vec, const uint64_t nedges, const uint32_t nedges_divisor, const std::string nedges_type);        
        void compress_tile(COMPRESSED_FORMAT compression_type, const REFINE_TYPE refine_type);
};

/* Process-based tiling based on MPI ranks*/ 
template<typename Weight>
Tiling<Weight>::Tiling(const uint32_t ntiles_, const uint32_t nrowgrps_, const uint32_t ncolgrps_, const uint32_t nranks_, 
                       const uint64_t nnz_, const uint32_t nrows_, const uint32_t ncols_,
                       const std::string input_file, const INPUT_TYPE input_type,
                       const TILING_TYPE tiling_type_, const COMPRESSED_FORMAT compression_type, const REFINE_TYPE refine_type) 
        : ntiles(ntiles_) , nrowgrps(nrowgrps_), ncolgrps(ncolgrps_), nranks(nranks_), rank_ntiles(ntiles_/nranks_), 
          nnz(nnz_), nrows(nrows_), ncols(ncols_), tiling_type(tiling_type_) {
            
    one_rank = ((nranks == 1) and (nranks != (uint32_t) Env::nranks)) ? true : false;
   
    if((rank_ntiles * nranks != ntiles) or (nrowgrps * ncolgrps != ntiles)) {
        Logging::print(Logging::LOG_LEVEL::ERROR, "Tiling failed\n");
        std::exit(Env::finalize()); 
    }
    
    if ((tiling_type == TILING_TYPE::_1D_ROW_)) {
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
    /*
    else if (tiling_type == TILING_TYPE::_2D_) {
        integer_factorize(nranks, rowgrp_nranks, colgrp_nranks);
        if(rowgrp_nranks * colgrp_nranks != nranks) {
            Logging::print(Logging::LOG_LEVEL::ERROR, "Tiling failed\n");
            std::exit(Env::finalize()); 
        }
    }
    */
    rank_nrowgrps = nrowgrps / colgrp_nranks;
    rank_ncolgrps = ncolgrps / rowgrp_nranks;        
    if(rank_nrowgrps * rank_ncolgrps != rank_ntiles) {
        Logging::print(Logging::LOG_LEVEL::ERROR, "Tiling failed\n");
        std::exit(Env::finalize()); 
    }
    
    nthreads = 1;//Env::nthreads; // I changed this in the past
    if ((tiling_type == TILING_TYPE::_1D_ROW_)) {
        rowgrp_nthreads = 1;
        colgrp_nthreads = nthreads;
        
    }
    else if (tiling_type == TILING_TYPE::_1D_COL_) {
        rowgrp_nthreads =  nthreads;
        colgrp_nthreads = 1;
        
    }
    /*
    else if (tiling_type == TILING_TYPE::_2D_) {
        rowgrp_nthreads = rowgrp_nranks;
        colgrp_nthreads = nrowgrps / rowgrp_nthreads;
    }
    */

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
    
/////    nrows += (nrows % nrowgrps) ? (nrowgrps - (nrows % nrowgrps)) : 0;
    
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
            tile.start_row = i*tile_height;
            tile.end_row = (i+1)*tile_height;
            tile.tile_height = tile_height;
            tile.start_col = i*tile_width;
            tile.end_col = (i+1)*tile_width;
            tile.tile_width = tile_width;
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
    
    if(INPUT_TYPE::_TEXT_ == input_type) {
        IO::text_file_read<Weight>(input_file, tiles, tile_height, tile_width, one_rank);
    }
    else {
        IO::binary_file_read<Weight>(input_file, tiles, tile_height, tile_width, one_rank);
    }
    
    if(not one_rank) {
        tile_exchange();
        //tile_load();
    }
    else {
        for (uint32_t i = 0; i < nrowgrps; i++) {
            for (uint32_t j = 0; j < ncolgrps; j++) {
                tiles[i][j].nedges = tiles[i][j].triples.size();
            }
        }
    }
    
    print_tiling("rank");
    print_tiling("nedges");
    //sort_tile(compression_type);
    repartition_tiles(compression_type);
    print_tiling("nedges");
    compress_tile(compression_type, refine_type);
    //std::exit(0);
}

template<typename Weight>
Tiling<Weight>::Tiling(const uint32_t ntiles_, const uint32_t nrowgrps_, const uint32_t ncolgrps_,  const uint32_t nranks_, 
                       const uint32_t rank_nthreads_, const uint32_t nthreads_, 
                       const uint64_t nnz_, const uint32_t nrows_, const uint32_t ncols_,
                       const std::string input_file, const INPUT_TYPE input_type,
                       const TILING_TYPE tiling_type_, const COMPRESSED_FORMAT compression_type, const REFINE_TYPE refine_type)
                     : ntiles(ntiles_) , nrowgrps(nrowgrps_), ncolgrps(ncolgrps_), nranks(nranks_), rank_ntiles(ntiles_/nranks_), 
                       rank_nthreads(rank_nthreads_), nthreads(nthreads_),
              //threads_ntiles(ntiles_), threads_nrowgrps(nrowgrps_), threads_ncolgrps(ncolgrps_), nthreads(nthreads_), thread_ntiles(ntiles_/nthreads_),
                       nnz(nnz_), nrows(nrows_), ncols(ncols_), tiling_type(tiling_type_) {
    
    one_rank = ((nranks == 1) and (nranks != (uint32_t) Env::nranks)) ? true : false;              
    
    if((rank_ntiles * nranks != ntiles) or (nrowgrps * ncolgrps != ntiles)) {
        Logging::print(Logging::LOG_LEVEL::ERROR, "Tiling failed\n");
        std::exit(Env::finalize()); 
    }
    
    if ((tiling_type == TILING_TYPE::_1D_ROW_)) {
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
    /*
    else if (tiling_type == TILING_TYPE::_2D_) {
        integer_factorize(nranks, rowgrp_nranks, colgrp_nranks);
        if(rowgrp_nranks * colgrp_nranks != nranks) {
            Logging::print(Logging::LOG_LEVEL::ERROR, "Tiling failed\n");
            std::exit(Env::finalize()); 
        }
    }
    */
    
    rank_nrowgrps = nrowgrps / colgrp_nranks;
    rank_ncolgrps = ncolgrps / rowgrp_nranks;        
    if(rank_nrowgrps * rank_ncolgrps != rank_ntiles) {
        Logging::print(Logging::LOG_LEVEL::ERROR, "Tiling failedd\n");
        std::exit(Env::finalize()); 
    }
    
    threads_ntiles = nthreads;
    thread_ntiles = threads_ntiles/nthreads;
    
    if ((tiling_type == TILING_TYPE::_1D_ROW_)) {
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

    if ((tiling_type == TILING_TYPE::_1D_ROW_)) {
        rowgrp_nthreads = 1;
        colgrp_nthreads = nthreads;
    }
    else if (tiling_type == TILING_TYPE::_1D_COL_) {
        rowgrp_nthreads =  nthreads;
        colgrp_nthreads = 1;
    }
    /*
    else if (tiling_type == TILING_TYPE::_2D_) {
        rowgrp_nthreads = rowgrp_nranks;
        colgrp_nthreads = nrowgrps / rowgrp_nthreads;
    }
    */
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
    
    //nrows = nrows_;
    ////nrows += (nrows % ntiles) ? (ntiles - (nrows % ntiles)) : 0;
    
    //ncols = ncols_;
   ///// ncols += (ncols % ntiles) ? (ntiles - (ncols % ntiles)) : 0;
    
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
            tile.tile_height = tile_height;
            tile.start_col = i*tile_width;
            tile.end_col = (i+1)*tile_width;
            tile.tile_width = tile_width;        
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
    
    if(INPUT_TYPE::_TEXT_ == input_type) {
        IO::text_file_read<Weight>(input_file, tiles, tile_height, tile_width, one_rank);
    }
    else {
        IO::binary_file_read<Weight>(input_file, tiles, tile_height, tile_width, one_rank);
    }
    
    if(not one_rank) {
        tile_exchange();
        tile_load();
    }
    else {
        for (uint32_t i = 0; i < nrowgrps; i++) {
            for (uint32_t j = 0; j < ncolgrps; j++) {
                tiles[i][j].rank = Env::rank;
                tiles[i][j].thread = j;
                tiles[i][j].nedges = tiles[i][j].triples.size();
            }
        }
    }
/*    
    for (uint32_t i = 0; i < nrowgrps; i++) {
        for (uint32_t j = 0; j < ncolgrps; j++) {
            auto& tile = tiles[i][j];
            if(tile.rank == Env::rank) {
                Env::tile_index[tile.thread] = i;
            }
        }
    }
*/    
    print_tiling("rank");
    print_tiling("thread");
    print_tiling("nedges");
   
    //sort_tile(compression_type);
    repartition_tiles(compression_type);
    print_tiling("nedges");
    printf("DONEE\n");
    std::exit(0);
    compress_tile(compression_type, refine_type);
}


template<typename Weight>
Tiling<Weight>::Tiling(const uint32_t ntiles_, const uint32_t nrowgrps_, const uint32_t ncolgrps_, const uint32_t nranks_, 
                       const uint32_t rank_nthreads_, const uint32_t nthreads_, 
                       const uint64_t nnz_, const uint32_t nrows_, const uint32_t ncols_,
                       const TILING_TYPE tiling_type_, const COMPRESSED_FORMAT compression_type, const REFINE_TYPE refine_type) 
                     : ntiles(ntiles_), nrowgrps(nrowgrps_), ncolgrps(ncolgrps_), nranks(nranks_), rank_ntiles(ntiles_/nranks_), 
                       nthreads(nthreads_),
                       nnz(nnz_), nrows(nrows_), ncols(ncols_), tile_height(nrows / nrowgrps), tile_width(ncols / ncolgrps), tiling_type(tiling_type_) {

    //one_rank = ((nranks == 1) and (nranks != (uint32_t) Env::nranks)) ? true : false;
    /*
    std::vector<uint64_t> ranks_nnz(nranks); 
    std::vector<uint32_t> ranks_nrows(nranks);    
    std::vector<uint32_t> ranks_ncols(nranks);    
    for (uint32_t r = 0; r < nranks; r++) {
        if (r != (uint32_t) Env::rank) {
            MPI_Sendrecv(&nnz, 1, MPI_UNSIGNED_LONG, r, 0, &ranks_nnz[r], 1, MPI_UNSIGNED_LONG,
                                                        r, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                                                        
            MPI_Sendrecv(&nrows, 1, MPI_UNSIGNED, r, 0, &ranks_nrows[r], 1, MPI_UNSIGNED,
                                                        r, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);                                                        
                                                        
            MPI_Sendrecv(&ncols, 1, MPI_UNSIGNED, r, 0, &ranks_ncols[r], 1, MPI_UNSIGNED,
                                                        r, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);                                                        
        }
        else {
            ranks_nnz[r] = nnz;
            ranks_nrows[r] = nrows;
            ranks_ncols[r] = ncols;
        }
    }
    */
    if((rank_ntiles * nranks != ntiles) or (nrowgrps * ncolgrps != ntiles)) {
        Logging::print(Logging::LOG_LEVEL::ERROR, "Tiling failed\n");
        std::exit(Env::finalize()); 
    }
    
    if ((tiling_type == TILING_TYPE::_1D_ROW_)) {
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
    
    //nthreads = Env::nthreads;
    threads_ntiles = nthreads;
    thread_ntiles = threads_ntiles/nthreads;
    //threads_nrowgrps = nthreads;
    //threads_ncolgrps = 1;
    
    if ((tiling_type == TILING_TYPE::_1D_ROW_)) {
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

    if ((tiling_type == TILING_TYPE::_1D_ROW_)) {
        rowgrp_nthreads = 1;
        colgrp_nthreads = nthreads;
    }
    else if (tiling_type == TILING_TYPE::_1D_COL_) {
        rowgrp_nthreads =  nthreads;
        colgrp_nthreads = 1;
    }
    /*
    else if (tiling_type == TILING_TYPE::_2D_) {
        rowgrp_nthreads = rowgrp_nranks;
        colgrp_nthreads = nrowgrps / rowgrp_nthreads;
    }
    */
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
    
    //nrows = nrows_;
    //nrows += (nrows % ntiles) ? (ntiles - (nrows % ntiles)) : 0;
    
    //ncols = ncols_;
    //ncols += (ncols % ntiles) ? (ntiles - (ncols % ntiles)) : 0;
    
    //tile_height = nrows / nrowgrps;
    //tile_width  = ncols / ncolgrps;
    
    /*
    tile_height = nrows;
    tile_width = ncols;
    
    if ((tiling_type == TILING_TYPE::_1D_ROW_)) {
        nnz = std::accumulate(ranks_nnz.begin(), ranks_nnz.end(), 0);
        nrows = std::accumulate(ranks_nrows.begin(), ranks_nrows.end(), 0);
        ncols = ncols;
    }
    else {
        Logging::print(Logging::LOG_LEVEL::ERROR, "Tiling failed\n");
        std::exit(Env::finalize()); 
    }
    */
    
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
            tile.tile_height = tile_height;
            tile.start_col = i*tile_width;
            tile.end_col = (i+1)*tile_width;
            tile.tile_width = tile_width;   
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
    
    
    /*
    Logging::print(Logging::LOG_LEVEL::INFO, "Tiling information: Process-based%s\n", TILING_TYPES[tiling_type]);
    Logging::print(Logging::LOG_LEVEL::INFO, "Tiling information: nrowgrps      x ncolgrps      = [%d x %d]\n", nrowgrps, ncolgrps);
    Logging::print(Logging::LOG_LEVEL::INFO, "Tiling information: rowgrp_nranks x colgrp_nranks = [%d x %d]\n", rowgrp_nranks, colgrp_nranks);
    Logging::print(Logging::LOG_LEVEL::INFO, "Tiling information: rank_nrowgrps x rank_ncolgrps = [%d x %d]\n", rank_nrowgrps, rank_ncolgrps);
    Logging::print(Logging::LOG_LEVEL::INFO, "Tiling information: nrows         x ncols         = [%d x %d]\n", nrows, ncols);
    Logging::print(Logging::LOG_LEVEL::INFO, "Tiling information: tile_height   x tile_width    = [%d x %d]\n", tile_height, tile_width);
    Logging::print(Logging::LOG_LEVEL::INFO, "Tiling information: nnz                           = [%d]\n", nnz);
    print_tiling("rank");
    */
    /*
    for (uint32_t i = 0; i < nrowgrps; i++) {
        for (uint32_t j = 0; j < ncolgrps; j++) {
            auto& tile = tiles[i][j];
            if(tile.rank == Env::rank) {
                tile.nedges = ranks_nnz[Env::rank];
            }
        }
    }
    */
    //tile_load();
    for (uint32_t i = 0; i < nrowgrps; i++) {
        for (uint32_t j = 0; j < ncolgrps; j++) {
            tiles[i][j].nedges = 0;
        }
    }
    
    
    print_tiling("rank");
    //print_tiling("thread");
    print_tiling("nedges");
    
    //sort_tile(compression_type);
    compress_tile(compression_type, refine_type);  
}


template<typename Weight>
Tiling<Weight>::Tiling(const uint32_t ntiles_, const uint32_t nrowgrps_, const uint32_t ncolgrps_, const uint32_t nranks_, 
                       const uint64_t nnz_, const uint32_t nrows_, const uint32_t ncols_,
                       const TILING_TYPE tiling_type_, const COMPRESSED_FORMAT compression_type, const REFINE_TYPE refine_type) 
                     : ntiles(ntiles_), nrowgrps(nrowgrps_), ncolgrps(ncolgrps_), nranks(nranks_), rank_ntiles(ntiles_/nranks_), 
                       //nthreads(nthreads_),
                       nnz(nnz_), nrows(nrows_), ncols(ncols_), tile_height(nrows / nrowgrps), tile_width(ncols / ncolgrps), tiling_type(tiling_type_) {

    //one_rank = ((nranks == 1) and (nranks != (uint32_t) Env::nranks)) ? true : false;

    if((rank_ntiles * nranks != ntiles) or (nrowgrps * ncolgrps != ntiles)) {
        Logging::print(Logging::LOG_LEVEL::ERROR, "Tiling failed\n");
        std::exit(Env::finalize()); 
    }
    
    if ((tiling_type == TILING_TYPE::_1D_ROW_)) {
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

    if ((tiling_type == TILING_TYPE::_1D_ROW_)) {
        rowgrp_nthreads = 1;
        colgrp_nthreads = nthreads;
    }
    else if (tiling_type == TILING_TYPE::_1D_COL_) {
        rowgrp_nthreads =  nthreads;
        colgrp_nthreads = 1;
    }
    /*
    else if (tiling_type == TILING_TYPE::_2D_) {
        rowgrp_nthreads = rowgrp_nranks;
        colgrp_nthreads = nrowgrps / rowgrp_nthreads;
    }
    */
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
    
   // nrows = nrows_;
    //nrows += (nrows % ntiles) ? (ntiles - (nrows % ntiles)) : 0;
    
    ///ncols = ncols_;
    //ncols += (ncols % ntiles) ? (ntiles - (ncols % ntiles)) : 0;
    
    //tile_height = nrows / nrowgrps;
    //tile_width  = ncols / ncolgrps;
    
    tiles.resize(nrowgrps);
    for (uint32_t i = 0; i < nrowgrps; i++) {
        tiles[i].resize(ncolgrps);
    }
    /*
    int32_t gcd_r = std::gcd(rowgrp_nranks, colgrp_nranks);
    int32_t gcd_t = std::gcd(rowgrp_nthreads, colgrp_nthreads);
    for (uint32_t i = 0; i < nrowgrps; i++) {
        for (uint32_t j = 0; j < ncolgrps; j++) {
            auto& tile = tiles[i][j];
            int32_t thread_rank = (((i % colgrp_nthreads) * rowgrp_nthreads + (j % rowgrp_nthreads)) 
                               + ((i / (nrowgrps/gcd_t)) * (thread_nrowgrps))) % (Env::nranks * Env::nthreads);
            tile.rank   = thread_rank % Env::nranks;
            tile.thread = thread_rank / Env::nranks;    
        }
    }
    */
    
    int32_t gcd_r = std::gcd(rowgrp_nranks, colgrp_nranks);
    for (uint32_t i = 0; i < nrowgrps; i++) {
        for (uint32_t j = 0; j < ncolgrps; j++) {
            auto& tile = tiles[i][j];
            tile.rank = (((i % colgrp_nranks) * rowgrp_nranks + (j % rowgrp_nranks)) + ((i / (nrowgrps/(gcd_r))) * (rank_nrowgrps))) % nranks;
            tile.start_row = i*tile_height;
            tile.end_row = (i+1)*tile_height;
            tile.tile_height = tile_height;
            tile.start_col = i*tile_width;
            tile.end_col = (i+1)*tile_width;
            tile.tile_width = tile_width;
        }
    }
    
    Logging::print(Logging::LOG_LEVEL::INFO, "Tiling Information: Thread-based%s\n", TILING_TYPES[tiling_type]);
    Logging::print(Logging::LOG_LEVEL::INFO, "Tiling information: nrowgrps      x ncolgrps      = [%d x %d]\n", nrowgrps, ncolgrps);
    Logging::print(Logging::LOG_LEVEL::INFO, "Tiling information: rowgrp_nranks x colgrp_nranks = [%d x %d]\n", rowgrp_nranks, colgrp_nranks);
    Logging::print(Logging::LOG_LEVEL::INFO, "Tiling information: rank_nrowgrps x rank_ncolgrps = [%d x %d]\n", rank_nrowgrps, rank_ncolgrps);
    //Logging::print(Logging::LOG_LEVEL::INFO, "Tiling Information: rowgrp_nthreads  x colgrp_nthreads  = [%d x %d]\n", rowgrp_nthreads, colgrp_nthreads);
    //Logging::print(Logging::LOG_LEVEL::INFO, "Tiling Information: thread_nrowgrps  x thread_ncolgrps  = [%d x %d]\n", thread_nrowgrps, thread_ncolgrps);
    Logging::print(Logging::LOG_LEVEL::INFO, "Tiling information: nrows            x ncols            = [%d x %d]\n", nrows, ncols);
    Logging::print(Logging::LOG_LEVEL::INFO, "Tiling information: tile_height      x tile_width       = [%d x %d]\n", tile_height, tile_width);
    Logging::print(Logging::LOG_LEVEL::INFO, "Tiling information: nnz                                 = [%d]\n", nnz);
    
    for (uint32_t i = 0; i < nrowgrps; i++) {
        for (uint32_t j = 0; j < ncolgrps; j++) {
            tiles[i][j].nedges = 0;
        }
    }
    
    //tile_load();
    
    print_tiling("rank");
    //print_tiling("thread");
    print_tiling("nedges");
    
    //sort_tile(compression_type);
    compress_tile(compression_type, refine_type);  
}


template<typename Weight>
void Tiling<Weight>::set_threads_indices() {
    for (uint32_t i = 0; i < nrowgrps; i++) {
        for (uint32_t j = 0; j < ncolgrps; j++) {
            auto& tile = tiles[i][j];
            if(tile.rank == Env::rank) {
                Env::tile_index[tile.thread] = i;
            }
        }
    } 
}



/*
template<typename Weight>
Tiling<Weight>::Tiling(const uint32_t ntiles_, const uint32_t nrowgrps_, const uint32_t ncolgrps_, 
                       const uint32_t nranks_, const uint32_t rank_nthreads_, const uint32_t nthreads_, 
                       const uint64_t nnz_, const uint32_t nrows_, const uint32_t ncols_,
                       const std::string input_file, const INPUT_TYPE input_type,
                       const TILING_TYPE tiling_type_, const COMPRESSED_FORMAT compression_type, const REFINE_TYPE refine_type)
            : ntiles(ntiles_) , nrowgrps(nrowgrps_), ncolgrps(ncolgrps_), nranks(nranks_), rank_ntiles(ntiles_/nranks_), rank_nthreads(rank_nthreads_), 
              threads_ntiles(ntiles_), threads_nrowgrps(nrowgrps_), threads_ncolgrps(ncolgrps_), nthreads(nthreads_), thread_ntiles(ntiles_/nthreads_),
              nnz(nnz_), nrows(nrows_), ncols(ncols_), tiling_type(tiling_type_) {

    one_rank = ((nranks == 1) and (nranks != (uint32_t) Env::nranks)) ? true : false;              
    
    if((rank_ntiles * nranks != ntiles) or (nrowgrps * ncolgrps != ntiles)) {
        Logging::print(Logging::LOG_LEVEL::ERROR, "Tiling failed\n");
        std::exit(Env::finalize()); 
    }
    
    if ((tiling_type == TILING_TYPE::_1D_ROW_)) {
        rowgrp_nranks = 1;
        colgrp_nranks = nranks;
        if(rowgrp_nranks * colgrp_nranks != nranks) {
            Logging::print(Logging::LOG_LEVEL::ERROR, "Tiling failed\n");
            std::exit(Env::finalize()); 
        }
    }
    else if(tiling_type == TILING_TYPE::_1D_COL_) {
        rowgrp_nranks = nranks;
        colgrp_nranks = 1;
        if(rowgrp_nranks * colgrp_nranks != nranks) {
            Logging::print(Logging::LOG_LEVEL::ERROR, "Tiling failed\n");
            std::exit(Env::finalize()); 
        }
    }
    else if (tiling_type == TILING_TYPE::_2D_) {
        integer_factorize(nranks, rowgrp_nranks, colgrp_nranks);
        if(rowgrp_nranks * colgrp_nranks != nranks) {
            Logging::print(Logging::LOG_LEVEL::ERROR, "Tiling failed\n");
            std::exit(Env::finalize()); 
        }
    }
    
    rank_nrowgrps = nrowgrps / colgrp_nranks;
    rank_ncolgrps = ncolgrps / rowgrp_nranks;        
    if(rank_nrowgrps * rank_ncolgrps != rank_ntiles) {
        Logging::print(Logging::LOG_LEVEL::ERROR, "Tiling failed\n");
        std::exit(Env::finalize()); 
    }

    if((thread_ntiles * nthreads != threads_ntiles) or (threads_nrowgrps * threads_ncolgrps != threads_ntiles)) {
        Logging::print(Logging::LOG_LEVEL::ERROR, "Tiling failed\n");
        std::exit(Env::finalize()); 
    }
    
    if ((tiling_type == TILING_TYPE::_1D_ROW_)) {
        rowgrp_nthreads = 1;
        colgrp_nthreads = nthreads;
    }
    else if (tiling_type == TILING_TYPE::_1D_COL_) {
        rowgrp_nthreads =  nthreads;
        colgrp_nthreads = 1;
    }
    else if (tiling_type == TILING_TYPE::_2D_) {
        rowgrp_nthreads = rowgrp_nranks;
        colgrp_nthreads = nrowgrps / rowgrp_nthreads;
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
    
    nrows = nrows_;
    nrows += (nrows % Env::nthreads) ? (Env::nthreads - (nrows % Env::nthreads)) : 0;
    
    ncols = ncols_;
    ncols += (ncols % Env::nthreads) ? (Env::nthreads - (ncols % Env::nthreads)) : 0;
    
    tile_height = nrows / threads_nrowgrps;
    tile_width  = ncols / threads_ncolgrps;
    
    
    tiles.resize(nrowgrps);
    for (uint32_t i = 0; i < nrowgrps; i++) {
        tiles[i].resize(ncolgrps);
    }
    
    int32_t gcd_r = std::gcd(rowgrp_nranks, colgrp_nranks);
    int32_t gcd_t = std::gcd(rowgrp_nthreads, colgrp_nthreads);
    for (uint32_t i = 0; i < threads_nrowgrps; i++) {
        for (uint32_t j = 0; j < threads_ncolgrps; j++) {
            auto& tile = tiles[i][j];
            tile.thread = (((i % colgrp_nthreads) * rowgrp_nthreads + (j % rowgrp_nthreads)) + ((i / (threads_nrowgrps/(gcd_t))) * (thread_nrowgrps))) % nthreads;
            tile.rank = (((i % colgrp_nranks) * rowgrp_nranks + (j % rowgrp_nranks)) + ((i / (nrowgrps/(gcd_r))) * (rank_nrowgrps))) % nranks;
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
    print_tiling("rank");
    print_tiling("thread");
  
    if(INPUT_TYPE::_TEXT_ == input_type) {
        IO::text_file_read<Weight>(input_file, tiles, tile_height, tile_width, one_rank);
    }
    else {
        IO::binary_file_read<Weight>(input_file, tiles, tile_height, tile_width, one_rank);
    }
   
    if(not one_rank) {
        tile_exchange();
        tile_load();
    }
    else {
        for (uint32_t i = 0; i < nrowgrps; i++) {
            for (uint32_t j = 0; j < ncolgrps; j++) {
                tiles[i][j].rank = Env::rank;
                tiles[i][j].nedges = tiles[i][j].triples.size();
            }
        }
        print_tiling("nedges");
    }
    compress_tile(compression_type, refine_type);
}

template<typename Weight>
Tiling<Weight>::Tiling(const uint32_t ntiles_, const uint32_t nrowgrps_, const uint32_t ncolgrps_, const uint32_t nranks_, 
                       const uint64_t nnz_, const uint32_t nrows_, const uint32_t ncols_,
                       const TILING_TYPE tiling_type_, const COMPRESSED_FORMAT compression_type, const REFINE_TYPE refine_type) 
                     : ntiles(ntiles_), nrowgrps(nrowgrps_), ncolgrps(ncolgrps_), nranks(nranks_), rank_ntiles(ntiles_/nranks_), 
                      nnz(nnz_), nrows(nrows_), ncols(ncols_), tiling_type(tiling_type_) {

    one_rank = ((nranks == 1) and (nranks != (uint32_t) Env::nranks)) ? true : false;
    
    std::vector<uint64_t> ranks_nnz(nranks); 
    std::vector<uint32_t> ranks_nrows(nranks);    
    std::vector<uint32_t> ranks_ncols(nranks);    
    for (uint32_t r = 0; r < nranks; r++) {
        if (r != (uint32_t) Env::rank) {
            MPI_Sendrecv(&nnz, 1, MPI_UNSIGNED_LONG, r, 0, &ranks_nnz[r], 1, MPI_UNSIGNED_LONG,
                                                        r, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                                                        
            MPI_Sendrecv(&nrows, 1, MPI_UNSIGNED, r, 0, &ranks_nrows[r], 1, MPI_UNSIGNED,
                                                        r, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);                                                        
                                                        
            MPI_Sendrecv(&ncols, 1, MPI_UNSIGNED, r, 0, &ranks_ncols[r], 1, MPI_UNSIGNED,
                                                        r, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);                                                        
        }
        else {
            ranks_nnz[r] = nnz;
            ranks_nrows[r] = nrows;
            ranks_ncols[r] = ncols;
        }
    }

    if((rank_ntiles * nranks != ntiles) or (nrowgrps * ncolgrps != ntiles)) {
        Logging::print(Logging::LOG_LEVEL::ERROR, "Tiling failed\n");
        std::exit(Env::finalize()); 
    }
    
    if ((tiling_type == TILING_TYPE::_1D_ROW_)) {
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
    
    tile_height = nrows;
    tile_width = ncols;
    if ((tiling_type == TILING_TYPE::_1D_ROW_)) {
        nnz = std::accumulate(ranks_nnz.begin(), ranks_nnz.end(), 0);
        nrows = std::accumulate(ranks_nrows.begin(), ranks_nrows.end(), 0);
        ncols = ncols;
    }
    else {
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
        }
    }
    
    Logging::print(Logging::LOG_LEVEL::INFO, "Tiling information: Process-based%s\n", TILING_TYPES[tiling_type]);
    Logging::print(Logging::LOG_LEVEL::INFO, "Tiling information: nrowgrps      x ncolgrps      = [%d x %d]\n", nrowgrps, ncolgrps);
    Logging::print(Logging::LOG_LEVEL::INFO, "Tiling information: rowgrp_nranks x colgrp_nranks = [%d x %d]\n", rowgrp_nranks, colgrp_nranks);
    Logging::print(Logging::LOG_LEVEL::INFO, "Tiling information: rank_nrowgrps x rank_ncolgrps = [%d x %d]\n", rank_nrowgrps, rank_ncolgrps);
    Logging::print(Logging::LOG_LEVEL::INFO, "Tiling information: nrows         x ncols         = [%d x %d]\n", nrows, ncols);
    Logging::print(Logging::LOG_LEVEL::INFO, "Tiling information: tile_height   x tile_width    = [%d x %d]\n", tile_height, tile_width);
    Logging::print(Logging::LOG_LEVEL::INFO, "Tiling information: nnz                           = [%d]\n", nnz);
    print_tiling("rank");
    
    for (uint32_t i = 0; i < nrowgrps; i++) {
        for (uint32_t j = 0; j < ncolgrps; j++) {
            auto& tile = tiles[i][j];
            if(tile.rank == Env::rank) {
                tile.nedges = ranks_nnz[Env::rank];
            }
        }
    }
    
    tile_load();
    compress_tile(compression_type, refine_type);  
}
*/

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
            if(field.compare("rank") == 0) 
                Logging::print(Logging::LOG_LEVEL::VOID, "%d ", tile.rank);
            else if(field.compare("thread") == 0) 
                Logging::print(Logging::LOG_LEVEL::VOID, "%d ", tile.thread);
            else if(field.compare("nedges") == 0) 
                Logging::print(Logging::LOG_LEVEL::VOID, "%d ", tile.nedges);
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
void Tiling<Weight>::insert_triple(const struct Triple<Weight> triple) {
    std::pair pair = std::make_pair((triple.row / tile_height), (triple.col / tile_width));
    tiles[pair.first][pair.second].triples.push_back(triple);
}

template<typename Weight>
void Tiling<Weight>::tile_exchange() {
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
    uint64_t exchange_size_local = 0;
    // Insert exchanged triples
    for (uint32_t r = 0; r < nranks; r++) {
        if (r != (uint32_t) Env::rank) {
            auto& inbox = inboxes[r];
            if(not inbox.empty()) {
                for(auto& triple: inbox) {
                    insert_triple(triple);
                }
                exchange_size_local += inbox.size();
                inbox.clear();
                inbox.shrink_to_fit();
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
    Logging::print(Logging::LOG_LEVEL::INFO, "Tile load: Start calculating imbalance.\n");
    Logging::print(Logging::LOG_LEVEL::INFO, "Tile load: Total number of edges = %lu\n", nedges);
    
    tile_load_print(rank_nedges, nedges, nranks, "rank");
    tile_load_print(rowgrp_nedges, nedges, nrowgrps, "row group");
    tile_load_print(colgrp_nedges, nedges, ncolgrps, "column group");

    Logging::print(Logging::LOG_LEVEL::INFO, "Tile load: Done calculating imbalance.\n");
    Env::barrier();
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
void Tiling<Weight>::sort_tile(COMPRESSED_FORMAT compression_type) {
    Env::barrier();
    
    Logging::print(Logging::LOG_LEVEL::INFO, "Tile sort: Start soring tiles\n");
    
    const RowSort<Weight> f_row;
    const ColSort<Weight> f_col;	
    
    for (uint32_t i = 0; i < nrowgrps; i++) {
        for (uint32_t j = 0; j < ncolgrps; j++) {
            auto& tile = tiles[i][j];
            if(tile.rank == Env::rank) {
                tile.sort(f_row, f_col, compression_type);
            }
        }
    }    

    Logging::print(Logging::LOG_LEVEL::INFO, "Tile sort: Done sorting tiles.\n");
    Env::barrier();
}

template<typename Weight>
void Tiling<Weight>::repartition_tiles(COMPRESSED_FORMAT compression_type) {
    Env::barrier();
    Logging::print(Logging::LOG_LEVEL::INFO, "Tile repartition: Start repartitioning tiles\n");
    
    const RowSort<Weight> f_row;
    for (uint32_t i = 0; i < nrowgrps; i++) {
        for (uint32_t j = 0; j < ncolgrps; j++) {
            auto& tile = tiles[i][j];
            if(tile.rank == Env::rank) {
                std::sort(tile.triples.begin(), tile.triples.end(), f_row); 
            }
        }
    }   
    
    uint64_t balanced_nnz_per_tile = nnz/ntiles;    
    
    MPI_Datatype MANY_TRIPLES;
    MPI_Type_contiguous(sizeof(Triple<Weight>), MPI_BYTE, &MANY_TRIPLES);
    MPI_Type_commit(&MANY_TRIPLES);
    
    MPI_Status status;   
    MPI_Request request;   
    std::vector<MPI_Request> requests;    
    
    std::vector<std::vector<uint32_t>> nnz_local(rank_nrowgrps, std::vector<uint32_t>(tile_height));
    std::vector<uint32_t> nnz_global(nrows);

    std::vector<struct Triple<Weight>> outbox;
    std::vector<struct Triple<Weight>> inbox;        
    uint32_t outbox_size = 0;
    uint32_t inbox_size = 0;

    std::vector<uint32_t> partitions_start(ntiles);
    std::vector<uint32_t> partitions_end(ntiles);
    std::vector<uint64_t> partitions_nnz(ntiles);
    std::vector<uint32_t> offsets(ntiles);
    
    
    uint32_t k = 0;
    for (uint32_t i = 0; i < nrowgrps; i++) {
        for (uint32_t j = 0; j < ncolgrps; j++) {
            auto& tile = tiles[i][j];
            if(tile.rank == Env::rank) {
                auto& triples = tile.triples;
                uint32_t row = triples[0].row;
                uint32_t col = 0;
                bool b = false;
                uint64_t n = 0;
                for(auto triple: triples) {
                    while((row != triple.row) and (row < ((i+1) * tile.tile_height))) {
                        nnz_local[k][row%tile_height] = col;
                        row++;
                        col = 0;
                    }
                    col++;
                }
                nnz_local[k][row%tile.tile_height] = col;
                k++;
            }
        }
    }
    
    std::vector<uint32_t> ks(nranks);
    for (uint32_t i = 0; i < nrowgrps; i++) {
        for (uint32_t j = 0; j < ncolgrps; j++) {
            auto& tile = tiles[i][j];
            auto& k = ks[tile.rank];
            offsets[(tile.rank*rank_nrowgrps)+k] = tile.start_row;
            k++;
            //if(!Env::rank) {
             //   printf("%d %d\n", tile.rank, tile.start_row);
            //}
        }
    }

    //if(Env::rank == 0) {
    //    printf("\n");
    //    for(auto o: offsets) {
    //        printf("%d\n", o);
    //    }
    //}
    
    if(Env::rank == 0) {
        for(uint32_t r = 1; r < nranks; r++) {
            for(uint32_t k = 0; k < rank_nrowgrps; k++) {
                MPI_Irecv(nnz_global.data() + offsets[(r*rank_nrowgrps)+k], tile_height, MPI_UNSIGNED, r, r, MPI_COMM_WORLD, &request);
                requests.push_back(request);
            }
        }
    } else {
        for(uint32_t k = 0; k < rank_nrowgrps; k++) {
            MPI_Isend(nnz_local[k].data(), nnz_local[k].size(), MPI_UNSIGNED, 0, Env::rank, MPI_COMM_WORLD, &request); 
            requests.push_back(request);
            //printf("%d %d %lu\n", Env::rank, k, nnz_local[k].size());
        }
    }
    
    MPI_Waitall(requests.size(), requests.data(), MPI_STATUSES_IGNORE);
    requests.clear();
    requests.shrink_to_fit();
    
    
    if(Env::rank == 0) {
        for(uint32_t k = 0; k < rank_nrowgrps; k++) {
            std::copy(nnz_local[k].begin(), nnz_local[k].end(), nnz_global.begin() + offsets[(Env::rank*rank_nrowgrps)+k]);
        }
        uint64_t global_sum_nnz = std::accumulate(nnz_global.begin(), nnz_global.end(), 0);
        //printf("Rank = %d GS = %lu\n", Env::rank, global_sum);
        if(global_sum_nnz != nnz) {
            Logging::print(Logging::LOG_LEVEL::ERROR, "Repartitioning error\n");
            std::exit(Env::finalize()); 
        }
        
        uint64_t n = 0;
        uint64_t m = 0;
        uint32_t start = 0;
        uint32_t end = 0;
        uint32_t t = 0;
        for(uint32_t i = 0; i < nrows; i++) {
            n += nnz_global[i];
            if(((int64_t) (balanced_nnz_per_tile - n) < 0) or ((i+1) == nrows)) {
                partitions_start[t] = (t == 0)         ? 0         : partitions_end[t-1];
                partitions_end[t]   = (t == ntiles-1)  ? nrows     : i;
                partitions_nnz[t]   = (t == ntiles-1)  ? (nnz - m) : (n - nnz_global[i]);
                i                   = ((i+1) == nrows) ? i         : (i - 1);
                m += partitions_nnz[t];
                n = 0;
                t++;
            }
        }
        /*
        uint64_t bb = 0;
        for(uint32_t i = 0; i < ntiles; i++) {
            uint64_t b = 0;
            
            for(uint32_t j = partitions_start[i]; j < partitions_end[i]; j++) {
                b += nnz_global[j];
            }
            bb += b;
            printf("%d %d %d %lu %lu\n", i, partitions_start[i], partitions_end[i], partitions_nnz[i], b);
        }
        */
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
        
        
        
        
        //printf("%lu %lu %lu\n", s, m, bb);
        
        
    }
    Env::barrier();
    
    
    
    
    printf("Rank = %d %lu\n", Env::rank, balanced_nnz_per_tile);
    Env::barrier();
    std::exit(0);
    
    /*

    
    nnz_local.clear();
    nnz_local.shrink_to_fit();    
    Env::barrier();

    if(Env::rank == 0) {
        for(uint32_t r = 1; r < nranks; r++) {
            MPI_Isend(partitions.data(), partitions.size(), MPI_UNSIGNED, r, Env::rank, MPI_COMM_WORLD, &request); 
            requests.push_back(request);
        }
        
        MPI_Waitall(requests.size(), requests.data(), MPI_STATUSES_IGNORE);
        requests.clear();
        requests.shrink_to_fit();
    }
    else {
        MPI_Recv(partitions.data(), partitions.size(), MPI_UNSIGNED, 0, 0, MPI_COMM_WORLD, &status);
    }
    
    Env::barrier();
    
    for (uint32_t i = 0; i < nrowgrps; i++) {
        for (uint32_t j = 0; j < ncolgrps; j++) {
            auto& tile = tiles[i][j];
            tile.start_row = partitions[tile.rank*2];
            tile.end_row = partitions[(tile.rank*2)+1];
            tile.tile_height = tile.end_row - tile.start_row;
            tile.start_col = 0;
            tile.end_col = tile_width;
            tile.tile_width = tile_width;
        }
    }

    for (uint32_t i = 0; i < nrowgrps; i++) {
        for (uint32_t j = 0; j < ncolgrps; j++) {
            auto& tile = tiles[i][j];
            if(tile.rank == Env::rank) {
                auto& triples = tile.triples;
                uint64_t n = triples.size()-1;
                while((n >= 0) and (triples[n].row >= tile.end_row)) n--;
                if(n < triples.size()-1) {
                    outbox.insert(outbox.begin(), triples.begin() + n + 1, triples.end());
                    triples.erase(triples.begin() + n + 1, triples.end());
                }
            }
        }
    }
    outbox_size = outbox.size();
    
    int32_t ring_next_rank = (Env::rank + 1) % nranks;    
    int32_t ring_prev_rank = (Env::rank - 1 + nranks) % nranks;

    if(ring_next_rank != 0) {
        MPI_Send(&outbox_size, 1, MPI_UNSIGNED, ring_next_rank, Env::rank, MPI_COMM_WORLD); 
    }
    if(ring_prev_rank != (int32_t) (nranks - 1)) {
        MPI_Recv(&inbox_size,  1, MPI_UNSIGNED, ring_prev_rank, ring_prev_rank, MPI_COMM_WORLD, &status);
    }
    
    if(ring_next_rank != 0) {
        MPI_Send(outbox.data(), outbox.size(), MANY_TRIPLES, ring_next_rank, Env::rank, MPI_COMM_WORLD); 
    }
    if(ring_prev_rank != (int32_t) (nranks - 1)) {
        inbox.resize(inbox_size);
        MPI_Recv(inbox.data(), inbox.size(), MANY_TRIPLES, ring_prev_rank, ring_prev_rank, MPI_COMM_WORLD, &status);
    }
    
    outbox.clear();
    outbox.shrink_to_fit();
    
    for (uint32_t i = 0; i < nrowgrps; i++) {
        for (uint32_t j = 0; j < ncolgrps; j++) {
            auto& tile = tiles[i][j];
            if((tile.rank == Env::rank) and (inbox.size())) {
                auto& triples = tile.triples;
                triples.insert(triples.end(), inbox.begin(), inbox.end());
            }
        }
    }
    
    inbox.clear();
    inbox.shrink_to_fit();
    
    std::vector<uint64_t> nedges_grid(nranks);
    for (uint32_t i = 0; i < nrowgrps; i++) {
        for (uint32_t j = 0; j < ncolgrps; j++) {
            auto& tile = tiles[i][j];
            if(tile.rank == Env::rank) {
                auto& triples = tile.triples;
                nedges_grid[Env::rank] = triples.size();
            }
        }
    }
    
    for(uint32_t r = 0; r < nranks; r++) {
        if(r != (uint32_t) Env::rank) {
            MPI_Sendrecv(&nedges_grid[Env::rank], 1, MPI_UNSIGNED_LONG, r, Env::rank, 
                         &nedges_grid[r], 1, MPI_UNSIGNED_LONG, r, r, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }
    }
    
    for(uint32_t i = 0; i < nrowgrps; i++) {
        for(uint32_t j = 0; j < ncolgrps; j++) {
            auto& tile = tiles[i][j];
            tile.nedges = nedges_grid[tile.rank];
        }
    }
    for(uint32_t i = 0; i < nrowgrps; i++) {
        for(uint32_t j = 0; j < ncolgrps; j++) {
            auto& tile = tiles[i][j];
            if(tile.rank == Env::rank) {
                tile_height = tile.end_row - tile.start_row;
            }
        }
    }

    auto retval = MPI_Type_free(&MANY_TRIPLES);
    if(retval != MPI_SUCCESS) {
        Logging::print(Logging::LOG_LEVEL::ERROR, "Tile repartitioning failed!\n");
        std::exit(Env::finalize()); 
    }
    Logging::print(Logging::LOG_LEVEL::INFO, "Tile repartition: New tile height %d.\n", tile_height);
    Logging::print(Logging::LOG_LEVEL::INFO, "Tile repartition: Done repartitioning tiles.\n");
    Env::barrier();
    */
}

template<typename Weight>
void Tiling<Weight>::compress_tile(COMPRESSED_FORMAT compression_type, const REFINE_TYPE refine_type) {
    Env::barrier();
    Logging::print(Logging::LOG_LEVEL::INFO, "Tile compression: Start compressing tile using %s \n", COMPRESSED_FORMATS[compression_type]);
    
    const RowSort<Weight> f_row;
    const ColSort<Weight> f_col;	
    
    for (uint32_t i = 0; i < nrowgrps; i++) {
        for (uint32_t j = 0; j < ncolgrps; j++) {
            auto& tile = tiles[i][j];
            if(tile.rank == Env::rank) {
                tile.sort(f_row, f_col, compression_type);
                tile.compress(tile.nedges, nrows, ncols, // tile_height, tile_width, 
                              compression_type, refine_type, one_rank);
            }
        }
    }    

    Logging::print(Logging::LOG_LEVEL::INFO, "Tile compression: Done compressing tiles with %s.\n", REFINE_TYPES[refine_type]);
    Env::barrier();      
}
#endif