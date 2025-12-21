#include "src/solver.h"

int main() {
    std::string basePath = ".././data/scheduling-benchmarks/wtsds/";  
    std::string instanceName = "wt_sds_1.instance";  

    std::string instancePath = basePath + instanceName;

    solver::ProblemInstance instance = solver::readInstance(instancePath);

    solver::Solution bestSolution = solver::simulatedAnnealing(instance, 100000, 1000.0, 0.995);
    std::cout << "Best TWT found: " << bestSolution.objectiveValue << std::endl;

    solver::Solution insertionSolution = solver::TWT_Insertion_Heuristic(instance);
    std::cout << "Insertion Heuristic TWT: " << insertionSolution.objectiveValue << std::endl;

    return 0;
}
