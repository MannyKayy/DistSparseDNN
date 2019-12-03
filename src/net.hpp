/*
 * net.hpp: Neural network base class 
 * (c) Mohammad Hasanzadeh Mofrad, 2019
 * (e) m.hasanzadeh.mofrad@gmail.com 
 */
 
#ifndef NET_HPP
#define NET_HPP

#include "triple.hpp"
#include "tiling.hpp"
#include "spops.hpp"
//#include "bitmap.hpp"

enum PARALLELISM_TYPE {_DATA_X_DATA_, _DATA_X_MODEL_};
const char* PARALLELISM_TYPES[] = {"_DATA_X_DATA_", "_DATA_X_MODEL_"};

template<typename Weight>
class Net {
    public:
        Net() {};
        ~Net() {};
        
        Net(const uint32_t NinputInstanses_, const uint32_t Nneurons_, 
            const std::string inputFile_prefix, const uint32_t maxLayers_, const std::string layerFile_prefix,
            const PARALLELISM_TYPE parallelism_type_  = PARALLELISM_TYPE::_DATA_X_DATA_,
            const INPUT_TYPE input_type = INPUT_TYPE::_BINARY_,
            const COMPRESSED_FORMAT compression_type = COMPRESSED_FORMAT::_CSC_);

        std::unique_ptr<struct Tiling<Weight>> inputFeatures = nullptr;
        std::vector<uint32_t> trueCategories;
        std::vector<std::unique_ptr<struct Tiling<Weight>>> layers;
        
        std::vector<std::shared_ptr<struct Data_Block<Weight>>> biasWeightVecs;
        std::vector<std::shared_ptr<struct Data_Block<bool>>> spaBoolVec;
        std::vector<std::shared_ptr<struct Data_Block<Weight>>> spaWeightVec;
        
        std::vector<struct Bitmap> spaBitmap;
        
        std::unique_ptr<struct Tiling<Weight>> output = nullptr;

        uint32_t NinputInstanses;        
        uint32_t Nneurons;
        Weight biasValue;
        uint32_t maxLayers;
        
        PARALLELISM_TYPE parallelism_type; 
        
        void inferenceReLU(COMPRESSED_FORMAT compression_type);
        void printTimes();
        void printTimesExcel();
        void execute();
        void inferenceReLU_t(const int32_t tid);
        //void validate_t(const int32_t tid);
};

template<typename Weight>
Net<Weight>::Net(const uint32_t NinputInstanses_, const uint32_t Nneurons_, 
                 const std::string inputFile_prefix, const uint32_t maxLayers_, const std::string layerFile_prefix,
                 const PARALLELISM_TYPE parallelism_type_, 
                 const INPUT_TYPE input_type, 
                 const COMPRESSED_FORMAT compression_type) : NinputInstanses(NinputInstanses_), Nneurons(Nneurons_), maxLayers(maxLayers_), parallelism_type(parallelism_type_) {
    
    auto start = std::chrono::high_resolution_clock::now();
    Logging::print(Logging::LOG_LEVEL::INFO, "Neural network: Processing input feature file for %d neurons and %s\n", Nneurons, PARALLELISM_TYPES[parallelism_type]);  
    std::vector<Weight> neuralNetBias = {-0.3,-0.35,-0.4,-0.45};
    std::vector<uint32_t> NneuronsVector = {1024, 4096, 16384, 65536};    
    uint32_t idxN = std::distance(NneuronsVector.begin(), std::find(NneuronsVector.begin(), NneuronsVector.end(), Nneurons));
    if(idxN >= NneuronsVector.size()) {
        Logging::print(Logging::LOG_LEVEL::ERROR, "Invalid number of neurons %d", Nneurons);
        std::exit(Env::finalize());
    }    
    biasValue = neuralNetBias[idxN];
    
    std::string feature_file = inputFile_prefix + "/sparse-images-" + std::to_string(Nneurons);
    feature_file += (input_type == INPUT_TYPE::_TEXT_) ? ".tsv" : ".bin";
    
    uint64_t nnz = 0;
    uint32_t nrows = 0;
    uint32_t ncols = 0;
    
    std::tie(nnz, nrows, ncols) = (INPUT_TYPE::_TEXT_ == input_type) ? IO::text_file_stat<Weight>(feature_file)
                                                                     : IO::binary_file_stat<Weight>(feature_file);

    nrows = ((NinputInstanses + 2) > nrows) ? (NinputInstanses + 2) : nrows; 
    ncols = ((Nneurons + 2) > ncols) ? (Nneurons + 2) : ncols;
    ncols += (ncols % Env::nthreads) ? (Env::nthreads - (ncols % Env::nthreads)) : 0;    
    //ncols += Env::nthreads; // Refine 
    
    if(parallelism_type == PARALLELISM_TYPE::_DATA_X_MODEL_) {
        inputFeatures = std::move(std::make_unique<Tiling<Weight>>(Env::nranks, Env::nranks, 1, Env::nranks, 
                                                                   nnz, nrows, ncols, 
                                                                   feature_file, input_type, 
                                                                   TILING_TYPE::_1D_ROW_, compression_type, REFINE_TYPE::_REFINE_NONE_));                                                         
    }
    else if (parallelism_type == PARALLELISM_TYPE::_DATA_X_DATA_) {                                                               
        inputFeatures = std::move(std::make_unique<Tiling<Weight>>(Env::nranks * Env::nthreads, Env::nranks * Env::nthreads, 1, Env::nranks,
                                                                   Env::nthreads, Env::nranks * Env::nthreads, 
                                                                   nnz, nrows, ncols, 
                                                                   feature_file, input_type, 
                                                                   TILING_TYPE::_1D_ROW_, compression_type, REFINE_TYPE::_REFINE_NONE_));
        inputFeatures->set_threads_indices();                                                           
    }
    
    Logging::print(Logging::LOG_LEVEL::INFO, "Neural network: Processing the category files for %d neurons and %d layers.\n", Nneurons, maxLayers); 
    std::vector<uint32_t> maxLayersVector = {120, 480, 1920};
    uint32_t idxL = std::distance(maxLayersVector.begin(), std::find(maxLayersVector.begin(), maxLayersVector.end(), maxLayers));
    if(idxL >= maxLayersVector.size()) {
        Logging::print(Logging::LOG_LEVEL::ERROR, "Invalid number of layers %d", maxLayers);
        std::exit(Env::finalize());
    }

    std::string categoryFile = layerFile_prefix + "/neuron" + std::to_string(Nneurons) + "-l" + std::to_string(maxLayers);
    categoryFile += (input_type == INPUT_TYPE::_TEXT_) ? "-categories.tsv" : "-categories.bin";
    
    if(INPUT_TYPE::_TEXT_ == input_type) {
        //IO::text_file_categories(categoryFile, trueCategories, inputFeatures->tile_height);
        IO::text_file_categories(categoryFile, trueCategories, inputFeatures->nrows);
    }
    else {
        //IO::binary_file_categories(categoryFile, trueCategories, inputFeatures->tile_height);
        IO::binary_file_categories(categoryFile, trueCategories, inputFeatures->nrows);
    }

    Logging::print(Logging::LOG_LEVEL::INFO, "Neural network: Processing %d layer files (silent).\n", maxLayers); 

    //maxLayers = 1;
    layers.resize(maxLayers);
    biasWeightVecs.resize(maxLayers);
    for(uint32_t i = 0; i < maxLayers; i++) {
        std::string layerFile = layerFile_prefix + "/neuron" + std::to_string(Nneurons) + "/n" + std::to_string(Nneurons) + "-l" + std::to_string(i+1);
        layerFile += (input_type == INPUT_TYPE::_TEXT_) ? ".tsv" : ".bin";

        std::tie(nnz, nrows, ncols) = (INPUT_TYPE::_TEXT_ == input_type) ? IO::text_file_stat<Weight>(layerFile)
                                                                     : IO::binary_file_stat<Weight>(layerFile);                                                                     
        nrows = (inputFeatures->ncols > nrows) ? inputFeatures->ncols : nrows; 
        ncols = (inputFeatures->ncols > ncols) ? inputFeatures->ncols : ncols; 
        
        if(parallelism_type == PARALLELISM_TYPE::_DATA_X_MODEL_) {
            layers[i] = std::move(std::make_unique<Tiling<Weight>>(Env::nthreads, 1, Env::nthreads, 1, 
                                                                   Env::nthreads, Env::nthreads, 
                                                                   nnz, nrows, ncols, 
                                                                   layerFile, input_type, 
                                                                   TILING_TYPE::_1D_COL_, compression_type, REFINE_TYPE::_REFINE_NONE_)); // REFINE_TYPE::_REFINE_BOTH_));                              
        }
        else if (parallelism_type == PARALLELISM_TYPE::_DATA_X_DATA_) {     
            layers[i] = std::move(std::make_unique<Tiling<Weight>>(1, 1, 1, 1, 
                                                                   nnz, nrows, ncols, 
                                                                   layerFile, input_type, 
                                                                   TILING_TYPE::_1D_COL_, compression_type, REFINE_TYPE::_REFINE_NONE_)); // REFINE_TYPE::_REFINE_BOTH_));                              
        }
                  
        biasWeightVecs[i] = std::move(std::make_shared<struct Data_Block<Weight>>(inputFeatures->ncols, Env::rank_socket_id));
        
        Logging::enabled = false; 
    }
    Logging::enabled = true;
    Logging::print(Logging::LOG_LEVEL::INFO, "Neural network: Done reading %d layer files.\n", maxLayers); 
    
    for(uint32_t i = 0; i < maxLayers; i++) {
        Weight* b_A = biasWeightVecs[i]->ptr;
        for(uint32_t i = 0; i < inputFeatures->ncols; i++) {
            b_A[i] = biasValue;
        }
    }
    
    spaWeightVec.resize(Env::nthreads);
    for(int32_t i = 0; i < Env::nthreads; i++) {
        spaWeightVec[i] = std::move(std::make_shared<struct Data_Block<Weight>>(inputFeatures->tile_height, Env::threads_socket_id[i]));
    }
    
    //printf("DOne %d %d\n", Env::rank, inputFeatures->tile_height);
    //std::exit(0);
    
    //
    //spaBoolVec
    
    /*
    for(int32_t i = 0; i < Env::nthreads; i++) {
        Env::rows[i].resize(inputFeatures->tile_height);
        Env::cols[i].resize(inputFeatures->tile_height);
    }
    */

    //output = std::move(std::make_unique<Tiling<Weight>>(Env::nranks, Env::nranks, 1, Env::nranks, 0, inputFeatures->tile_height, layers[0]->ncols, 
    //                                                    TILING_TYPE::_1D_ROW_, compression_type)); 
    if(parallelism_type == PARALLELISM_TYPE::_DATA_X_MODEL_) {
        
        output = std::move(std::make_unique<Tiling<Weight>>(Env::nranks, Env::nranks, 1, Env::nranks, 
                                                            0, inputFeatures->nrows, inputFeatures->ncols, 
                                                            TILING_TYPE::_1D_ROW_, compression_type, REFINE_TYPE::_REFINE_NONE_));
                                                            //printf("1._DATA_X_MODEL_\n");
                                                           //std::exit(0);
    }
    else if (parallelism_type == PARALLELISM_TYPE::_DATA_X_DATA_) { 
        output = std::move(std::make_unique<Tiling<Weight>>(Env::nranks * Env::nthreads, Env::nranks * Env::nthreads, 1, Env::nranks, 
                                                            Env::nthreads, Env::nranks * Env::nthreads, 
                                                            0, inputFeatures->nrows, inputFeatures->ncols, 
                                                            TILING_TYPE::_1D_ROW_, compression_type, REFINE_TYPE::_REFINE_NONE_)); 
                                                            //printf("1._DATA_X_DATA_\n");
    }
    
    
    //spaBitmap.resize(Env::nthreads);
    //for(int32_t i = 0; i < Env::nthreads; i++)
    //    spaBitmap[i] = Bitmap(inputFeatures->tile_height);
    
    Logging::print(Logging::LOG_LEVEL::INFO, "Neural network: Running the inferenceReLU method.\n"); 
    Env::barrier();
    //auto start = std::chrono::high_resolution_clock::now();
    //inferenceReLU(compression_type);
    
    execute();

    //auto finish = std::chrono::high_resolution_clock::now();
    //Env::exec_time = (double)(std::chrono::duration_cast< std::chrono::nanoseconds>(finish-start1).count())/1e9;
    
    /*
    auto& C_tile = inputFeatures->tiles[Env::rank][0];    
    auto& C_spmat = C_tile.spmat;
    bool passed = validate_prediction(C_spmat, trueCategories);
    if(passed) {
        Logging::print(Logging::LOG_LEVEL::INFO, "Challenge PASSED.\n");
    }
    else {
        Logging::print(Logging::LOG_LEVEL::ERROR, "Challenge FAILED.\n");
    }
    */
    auto finish = std::chrono::high_resolution_clock::now();
    Env::end_to_end_time = (double)(std::chrono::duration_cast< std::chrono::nanoseconds>(finish-start).count())/1e9;
    Env::barrier();
    printTimesExcel();
    //printTimes();
    
}

template<typename Weight>
void Net<Weight>::printTimes() {
    Env::barrier();
    double sum = 0.0, mean = 0.0, std_dev = 0.0, min = 0.0, max = 0.0;
    const char* TIME_MSGS[] = {"I/O          ", "SpMM Symbolic", "SpMM Real    ", "Mem realloc  ", "Execution    ", "Total Run    "};
    const double TIME_VALUES[] = {Env::io_time, Env::spmm_sym_time, Env::spmm_time, Env::memory_time, Env::exec_time, Env::end_to_end_time};
    const int32_t n = 6;
    
    for(int32_t i = 0; i < n; i++) {
        std::tie(sum, mean, std_dev, min, max) =  Env::statistics<double>(TIME_VALUES[i]);
        Logging::print(Logging::LOG_LEVEL::INFO, "%s time (sec): avg +/- std_dev: %.3f +/- %.3f | min: %.3f | max: %.3f\n", TIME_MSGS[i], mean, std_dev, min, max);
    }
    std::vector<double> sum_time; 
    std::vector<double> mean_time;
    std::vector<double> std_dev_time;
    std::vector<double> min_time;
    std::vector<double> max_time;
    Logging::print(Logging::LOG_LEVEL::VOID, "time | nnz | nnz_i\n");
    Logging::print(Logging::LOG_LEVEL::VOID, "l mean std_dev min max mean std_dev min max mean std_dev min max\n");
    for (uint32_t l = 0; l < maxLayers; l++) {
        std::tie(sum, mean, std_dev, min, max) =  Env::statistics<double>(Env::time_ranks[l]);
        Logging::print(Logging::LOG_LEVEL::VOID, "%3d %.3f %.3f %.3f %.3f ", l, mean, std_dev, min, max);
        uint64_t sum1 = 0.0, mean1 = 0.0, std_dev1 = 0.0, min1 = 0.0, max1 = 0.0;
        std::tie(sum1, mean1, std_dev1, min1, max1) =  Env::statistics<uint64_t>(Env::nnz_ranks[l]);
        Logging::print(Logging::LOG_LEVEL::VOID, "%12lu %12lu %12lu %12lu ", mean1, std_dev1, min1, max1);
        std::tie(sum1, mean1, std_dev1, min1, max1) =  Env::statistics<uint64_t>(Env::nnz_i_ranks[l]);
        Logging::print(Logging::LOG_LEVEL::VOID, "%12lu %12lu %12lu %12lu ", mean1, std_dev1, min1, max1);
        std::tie(sum1, mean1, std_dev1, min1, max1) =  Env::statistics<uint64_t>(Env::nnz_mean_thread_ranks[l]);
        Logging::print(Logging::LOG_LEVEL::VOID, "%12lu %12lu %12lu %12lu ", mean1, std_dev1, min1, max1);
        std::tie(sum1, mean1, std_dev1, min1, max1) =  Env::statistics<uint64_t>(Env::nnz_std_dev_thread_ranks[l]);
        Logging::print(Logging::LOG_LEVEL::VOID, "%12lu %12lu %12lu %12lu ", mean1, std_dev1, min1, max1);
        std::tie(sum1, mean1, std_dev1, min1, max1) =  Env::statistics<uint64_t>(Env::nnz_i_mean_thread_ranks[l]);
        Logging::print(Logging::LOG_LEVEL::VOID, "%12lu %12lu %12lu %12lu ", mean1, std_dev1, min1, max1);        
        std::tie(sum1, mean1, std_dev1, min1, max1) =  Env::statistics<uint64_t>(Env::nnz_i_std_dev_thread_ranks[l]);
        Logging::print(Logging::LOG_LEVEL::VOID, "%12lu %12lu %12lu %12lu\n", mean1, std_dev1, min1, max1);        
        
        Env::barrier();
    }
}

template<typename Weight>
void Net<Weight>::printTimesExcel() {
    Env::barrier();
    Logging::print(Logging::LOG_LEVEL::VOID, "exec: mean, std_dev, min, max, spmm_sym_mean, spmm_mean, mem_mean\n");
    double sum = 0.0, mean = 0.0, std_dev = 0.0, min = 0.0, max = 0.0;
    //std::tie(sum, mean, std_dev, min, max) =  Env::statistics<double>(Env::exec_time);
    std::tie(sum, mean, std_dev, min, max) =  Env::statistics_t<double>(Env::execution_time);
    Logging::print(Logging::LOG_LEVEL::VOID, "time: %.3f %.3f %.3f %.3f ", mean, std_dev, min, max);
    //std::tie(sum, mean, std_dev, min, max) =  Env::statistics<double>(Env::spmm_sym_time);
    std::tie(sum, mean, std_dev, min, max) =  Env::statistics_t<double>(Env::spmm_symb_time);
    Logging::print(Logging::LOG_LEVEL::VOID, "%.3f ", mean);
    //std::tie(sum, mean, std_dev, min, max) =  Env::statistics<double>(Env::spmm_time);
    std::tie(sum, mean, std_dev, min, max) =  Env::statistics_t<double>(Env::spmm_real_time);
    Logging::print(Logging::LOG_LEVEL::VOID, "%.3f ", mean);
    //std::tie(sum, mean, std_dev, min, max) =  Env::statistics<double>(Env::memory_time);
    std::tie(sum, mean, std_dev, min, max) =  Env::statistics_t<double>(Env::memory_allocation_time);
    Logging::print(Logging::LOG_LEVEL::VOID, "%.3f\n", mean);
}

template<typename Weight>
void Net<Weight>::execute() {
    std::vector<std::thread> threads;
    
    //auto start = std::chrono::high_resolution_clock::now();
    for(int i = 0; i < Env::nthreads; i++) {
        threads.push_back(std::thread(&Net<Weight>::inferenceReLU_t, this, i));
    }
    //auto finish = std::chrono::high_resolution_clock::now();
    //Env::exec_time = (double)(std::chrono::duration_cast< std::chrono::nanoseconds>(finish-start).count())/1e9;

    
    for(std::thread& th: threads) {
        th.join();
    }
}

//template<typename Weight>
//void Net<Weight>::validate_t(const int32_t tid) {
        
  //  printf(">>>> %d\n", tid);
    /*
    auto& C_tile = inputFeatures->tiles[Env::rank][0];    
    auto& C_spmat = C_tile.spmat;
    bool passed = validate_prediction(C_spmat, trueCategories);
    if(passed) {
        Logging::print(Logging::LOG_LEVEL::INFO, "Challenge PASSED.\n");
    }
    else {
        Logging::print(Logging::LOG_LEVEL::ERROR, "Challenge FAILED.\n");
    }
    */
    
    
    
//}

template<typename Weight>
void Net<Weight>::inferenceReLU_t(const int32_t tid) {
    bool ret = Env::set_thread_affinity(tid);   
    //std::chrono::system_clock::time_point start_time;
    double layer_time;
    
    uint64_t nnz = 0;
    uint32_t nrows = inputFeatures->tile_height;
    uint32_t ncols = layers[0]->ncols;
    
    //Env::assign_col(ncols, tid);
    
    struct Tile<Weight> A_tile;
    struct Tile<Weight> B_tile;
    struct Tile<Weight> C_tile;
    auto& s_spa = spaWeightVec[tid];

    if(parallelism_type == PARALLELISM_TYPE::_DATA_X_MODEL_) {
        Env::assign_col(ncols, tid);
        auto start_time = std::chrono::high_resolution_clock::now();  
        for (uint32_t l = 0; l < maxLayers; l++) {
            A_tile = inputFeatures->tiles[Env::rank][0];
            auto& A_spmat = A_tile.spmat;
            B_tile = layers[l]->tiles[0][tid];
            auto& B_spmat = B_tile.spmat;
            std::tie(Env::offset_nnz[tid], std::ignore, std::ignore) =  spmm_sym(A_spmat, B_spmat, s_spa, tid);
            
            pthread_barrier_wait(&Env::thread_barrier);
            if(!tid) nnz = Env::assign_nnz();
            
            C_tile = output->tiles[Env::rank][0];
            auto& C_spmat = C_tile.spmat;
            auto& b_bias = biasWeightVecs[l];
            if(!tid) {
                C_spmat->reallocate(nnz, nrows, ncols);
            }
            pthread_barrier_wait(&Env::thread_barrier);
            
            spmm(A_spmat, B_spmat, C_spmat, s_spa, b_bias, tid);
            pthread_barrier_wait(&Env::thread_barrier);
            adjust(C_spmat, tid);
            repopulate(A_spmat, C_spmat, tid);
            if(!tid) Env::iteration++;
        }    
        
        auto finish_time = std::chrono::high_resolution_clock::now();
        Env::execution_time[tid] = (double)(std::chrono::duration_cast< std::chrono::nanoseconds>(finish_time - start_time).count())/1e9    ;

        C_tile = inputFeatures->tiles[Env::rank][0];
        auto& C_spmat = C_tile.spmat;
        if(!tid) {
            bool passed = validate_prediction(C_spmat, trueCategories);
            if(passed) {
                Logging::print(Logging::LOG_LEVEL::INFO, "Challenge PASSED.\n");
            }
            else {
                Logging::print(Logging::LOG_LEVEL::ERROR, "Challenge FAILED.\n");
            }
        }
        pthread_barrier_wait(&Env::thread_barrier);
        
    }
    else if (parallelism_type  == PARALLELISM_TYPE::_DATA_X_DATA_) {
        auto start_time = std::chrono::high_resolution_clock::now();  
        for (uint32_t l = 0; l < maxLayers; l++) {
            if(not(l%2)) {
                A_tile = inputFeatures->tiles[Env::tile_index[tid]][0];
                C_tile = output->tiles[Env::tile_index[tid]][0];
            }
            else {
                A_tile = output->tiles[Env::tile_index[tid]][0];
                C_tile = inputFeatures->tiles[Env::tile_index[tid]][0];
            }

            auto& A_spmat = A_tile.spmat;
            auto& C_spmat = C_tile.spmat;
            B_tile = layers[l]->tiles[0][0];
            auto& B_spmat = B_tile.spmat;
            auto& b_bias = biasWeightVecs[l];  
            std::tie(nnz, std::ignore, std::ignore) =  spmm_sym(A_spmat, B_spmat, s_spa, tid);
            Env::index_nnz[tid] = 0;
            C_spmat->reallocate(nnz, nrows, ncols, tid);
            spmm(A_spmat, B_spmat, C_spmat, s_spa, b_bias, tid);
            if(!tid) Env::iteration++;
        }
        
        auto finish_time = std::chrono::high_resolution_clock::now();
        Env::execution_time[tid] = (double)(std::chrono::duration_cast< std::chrono::nanoseconds>(finish_time - start_time).count())/1e9;
        
        C_tile = inputFeatures->tiles[Env::tile_index[tid]][0];
        auto& C_spmat = C_tile.spmat;
        bool passed = validate_prediction(C_spmat, trueCategories, tid);
        if(passed) {
            if(!tid) Logging::print(Logging::LOG_LEVEL::INFO, "Challenge PASSED.\n");
        }
        else {
            Logging::print(Logging::LOG_LEVEL::ERROR, "Challenge FAILED.\n");
        }
        
        pthread_barrier_wait(&Env::thread_barrier);        
    }
    /*
    auto finish_time = std::chrono::high_resolution_clock::now();
    Env::execution_time[tid] = (double)(std::chrono::duration_cast< std::chrono::nanoseconds>(finish_time - start_time).count())/1e9;
    //pthread_barrier_wait(&Env::thread_barrier);
    bool passed =false;
    if(parallelism_type == PARALLELISM_TYPE::_DATA_X_MODEL_) {
        C_tile = inputFeatures->tiles[Env::rank][0];
        auto& C_spmat = C_tile.spmat;
        if(!tid) {
            passed = validate_prediction(C_spmat, trueCategories);
            if(passed) {
                Logging::print(Logging::LOG_LEVEL::INFO, "Challenge PASSED.\n");
            }
            else {
                Logging::print(Logging::LOG_LEVEL::ERROR, "Challenge FAILED.\n");
            }
        }
        pthread_barrier_wait(&Env::thread_barrier);
    }
    else if (parallelism_type  == PARALLELISM_TYPE::_DATA_X_DATA_) {
    //printf("VALIDATE\n");
        C_tile = inputFeatures->tiles[Env::tile_index[tid]][0];
        auto& C_spmat = C_tile.spmat;
        passed = validate_prediction(C_spmat, trueCategories, tid);
        if(passed) {
                if(!tid) {
                    Logging::print(Logging::LOG_LEVEL::INFO, "Challenge PASSED.\n");
                }
        else {
                Logging::print(Logging::LOG_LEVEL::ERROR, "Challenge FAILED.\n");
        }
        pthread_barrier_wait(&Env::thread_barrier);        
    }    
    //printf("%d %d %d\n", Env::rank, tid, passed);
    
    }
    */
    
}

/*
template<typename Weight>
void Net<Weight>::inferenceReLU(COMPRESSED_FORMAT compression_type) {
    uint64_t nnz = 0;
    uint32_t nrows = inputFeatures->tile_height;
    uint32_t ncols = layers[0]->ncols;
    
    #pragma omp parallel
    {
        // Layer 0
        int tid = omp_get_thread_num();
        
        double start_time;
        if(!tid) {
            start_time = Env::tic();                                                                    
        }
        Env::assign_col(ncols, tid);
        
        auto& A0_tile = inputFeatures->tiles[Env::rank][0];
        auto& A0_spmat = A0_tile.spmat;
        auto& B0_tile = layers[0]->tiles[0][0];
        auto& B0_spmat = B0_tile.spmat;
        auto& s_spa = spaDenseVec[tid];
        //auto& s_spa_bitmap = spaBitmap[tid];

        //std::tie(Env::offset_nnz[tid], std::ignore, std::ignore) = spmm_sym(A0_spmat, B0_spmat, s_spa_bitmap, tid);                                              
        std::tie(Env::offset_nnz[tid], std::ignore, std::ignore) = spmm_sym(A0_spmat, B0_spmat, s_spa, tid);                                              
        Env::count_nnz[tid] = Env::offset_nnz[tid];
        #pragma omp barrier
        if(!tid) {
            nnz = Env::assign_nnz();
            //output = std::move(std::make_unique<Tiling<Weight>>(Env::nranks, Env::nranks, 1, Env::nranks, nnz, nrows, ncols, 
            //                                                    TILING_TYPE::_1D_ROW_, compression_type)); 
        }

        #pragma omp barrier
        auto& C0_tile = output->tiles[Env::rank][0];    
        auto& C0_spmat = C0_tile.spmat;
        if(!tid) {
            C0_spmat->reallocate(nnz, nrows, ncols);
        }
        #pragma omp barrier
        auto& b_bias = biasDenseVecs[0];
        //spmm(A0_spmat, B0_spmat, C0_spmat, s_spa_bitmap, s_spa, b_bias, tid);
        spmm(A0_spmat, B0_spmat, C0_spmat, s_spa, b_bias, tid);
        
        if(!tid) {
            Env::iteration++;
            Env::time_ranks.push_back(Env::toc(start_time));
        }
        
        // Layer 1 to the last layer
        struct Tile<Weight> A_tile;
        struct Tile<Weight> B_tile;
        struct Tile<Weight> C_tile;
        for (uint32_t l = 1; l < maxLayers; l++) {
            if(!tid) {
                start_time = Env::tic();                                                                    
            }
        
            if(not(l%2)) {
                A_tile = inputFeatures->tiles[Env::rank][0];
                C_tile = output->tiles[Env::rank][0];
            }
            else {
                A_tile = output->tiles[Env::rank][0];
                C_tile = inputFeatures->tiles[Env::rank][0];
            }

            auto& A_spmat = A_tile.spmat;
            auto& C_spmat = C_tile.spmat;
            B_tile = layers[l]->tiles[0][0];
            auto& B_spmat = B_tile.spmat;
            
            auto& b_bias = biasDenseVecs[l];  
            //std::tie(Env::offset_nnz[tid], std::ignore, std::ignore) =  spmm_sym(A_spmat, B_spmat, s_spa_bitmap, tid);
            std::tie(Env::offset_nnz[tid], std::ignore, std::ignore) =  spmm_sym(A_spmat, B_spmat, s_spa, tid);
            Env::count_nnz[tid] = Env::offset_nnz[tid];
            #pragma omp barrier
            if(!tid) {
                nnz = Env::assign_nnz();
                //if(Env::iteration < 2)
                C_spmat->reallocate(nnz, nrows, ncols);
            }
            #pragma omp barrier
            //spmm(A_spmat, B_spmat, C_spmat, s_spa_bitmap, s_spa, b_bias, tid);
            spmm(A_spmat, B_spmat, C_spmat, s_spa, b_bias, tid);
            
            if(!tid) {
                Env::iteration++;
                Env::time_ranks.push_back(Env::toc(start_time));
            }
        }
    }
}
*/
#endif 
