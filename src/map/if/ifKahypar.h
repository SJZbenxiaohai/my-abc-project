/**CFile****************************************************************

  FileName    [ifKahypar.h]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [FPGA mapping based on priority cuts with hypergraph partitioning.]

  Synopsis    [KaHyPar hypergraph partitioning interface.]

  Author      [Based on LSOracle implementation]
  
  Affiliation [ABC team]

  Date        [Ver. 1.0. Started - January 2025.]

***********************************************************************/

#ifndef ABC__map__if__ifKahypar_h
#define ABC__map__if__ifKahypar_h

////////////////////////////////////////////////////////////////////////
///                          INCLUDES                                ///
////////////////////////////////////////////////////////////////////////

#include "if.h"
#include "ifHyperAig.h"

ABC_NAMESPACE_HEADER_START

////////////////////////////////////////////////////////////////////////
///                         PARAMETERS                               ///
////////////////////////////////////////////////////////////////////////

// Default KaHyPar configuration (based on LSOracle settings)
#define KAHYPAR_DEFAULT_CONFIG_STR \
"# general\n" \
"mode=direct\n" \
"objective=km1\n" \
"seed=-1\n" \
"cmaxnet=1000\n" \
"vcycles=0\n" \
"# main -> preprocessing -> min hash sparsifier\n" \
"p-use-sparsifier=true\n" \
"p-sparsifier-min-median-he-size=28\n" \
"p-sparsifier-max-hyperedge-size=1200\n" \
"p-sparsifier-max-cluster-size=10\n" \
"p-sparsifier-min-cluster-size=2\n" \
"p-sparsifier-num-hash-func=5\n" \
"p-sparsifier-combined-num-hash-func=100\n" \
"# main -> preprocessing -> community detection\n" \
"p-detect-communities=true\n" \
"p-detect-communities-in-ip=true\n" \
"p-reuse-communities=false\n" \
"p-max-louvain-pass-iterations=100\n" \
"p-min-eps-improvement=0.0001\n" \
"p-louvain-edge-weight=hybrid\n" \
"# main -> coarsening\n" \
"c-type=ml_style\n" \
"c-s=1\n" \
"c-t=160\n" \
"# main -> coarsening -> rating\n" \
"c-rating-score=heavy_edge\n" \
"c-rating-use-communities=true\n" \
"c-rating-heavy_node_penalty=no_penalty\n" \
"c-rating-acceptance-criterion=best_prefer_unmatched\n" \
"c-fixed-vertex-acceptance-criterion=fixed_vertex_allowed\n" \
"# main -> initial partitioning\n" \
"i-mode=recursive\n" \
"i-technique=multi\n" \
"# initial partitioning -> coarsening\n" \
"i-c-type=ml_style\n" \
"i-c-s=1\n" \
"i-c-t=150\n" \
"# initial partitioning -> coarsening -> rating\n" \
"i-c-rating-score=heavy_edge\n" \
"i-c-rating-use-communities=true\n" \
"i-c-rating-heavy_node_penalty=no_penalty\n" \
"i-c-rating-acceptance-criterion=best_prefer_unmatched\n" \
"i-c-fixed-vertex-acceptance-criterion=fixed_vertex_allowed\n" \
"# initial partitioning -> initial partitioning\n" \
"i-algo=pool\n" \
"i-runs=20\n" \
"# initial partitioning -> bin packing\n" \
"i-bp-algorithm=worst_fit\n" \
"i-bp-heuristic-prepacking=false\n" \
"i-bp-early-restart=true\n" \
"i-bp-late-restart=true\n" \
"# initial partitioning -> local search\n" \
"i-r-type=twoway_fm\n" \
"i-r-runs=-1\n" \
"i-r-fm-stop=simple\n" \
"i-r-fm-stop-i=50\n" \
"# main -> local search\n" \
"r-type=kway_fm_hyperflow_cutter_km1\n" \
"r-runs=-1\n" \
"r-fm-stop=adaptive_opt\n" \
"r-fm-stop-alpha=1\n" \
"r-fm-stop-i=350\n" \
"# local_search -> flow scheduling and heuristics\n" \
"r-flow-execution-policy=exponential\n" \
"# local_search -> hyperflowcutter configuration\n" \
"r-hfc-size-constraint=mf-style\n" \
"r-hfc-scaling=16\n" \
"r-hfc-distance-based-piercing=true\n" \
"r-hfc-mbc=true\n"

////////////////////////////////////////////////////////////////////////
///                         BASIC TYPES                              ///
////////////////////////////////////////////////////////////////////////

typedef struct Kahypar_Par_t_ Kahypar_Par_t;

// KaHyPar partitioning parameters
struct Kahypar_Par_t_
{
    int                    nPartitions;       // number of partitions (default: 2)
    double                 dImbalance;        // imbalance factor (default: 0.9)
    char *                 pConfigFile;       // path to KaHyPar config file (optional)
    int                    fVerbose;          // verbose output
    int                    fUseNodeWeights;   // use node weights
    int                    fUseEdgeWeights;   // use edge weights
};

typedef struct Kahypar_Result_t_ Kahypar_Result_t;

// KaHyPar partitioning result
struct Kahypar_Result_t_
{
    int                    nVertices;         // number of vertices
    int                    nPartitions;       // number of partitions
    Vec_Int_t *            vPartition;        // partition assignment for each vertex
    int                    nCutEdges;         // number of cut hyperedges
    int                    fSuccess;          // partitioning success flag
};

////////////////////////////////////////////////////////////////////////
///                      MACRO DEFINITIONS                           ///
////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////
///                    FUNCTION DECLARATIONS                         ///
////////////////////////////////////////////////////////////////////////

/*=== ifKahypar.c ==========================================================*/
extern Kahypar_Par_t *     Kahypar_ParAlloc();
extern void                Kahypar_ParFree( Kahypar_Par_t * p );
extern void                Kahypar_ParSetDefault( Kahypar_Par_t * p );
extern Kahypar_Result_t *  Kahypar_ResultAlloc( int nVertices );
extern void                Kahypar_ResultFree( Kahypar_Result_t * p );
extern int                 Kahypar_CreateTempConfig( char * pFileName );
extern Kahypar_Result_t *  Kahypar_PartitionHypergraph( Aig_Hyper_t * pHyper, Kahypar_Par_t * pPars );
extern void                Kahypar_PrintResult( Kahypar_Result_t * pResult );
extern int                 Kahypar_TestPartition( void * pNtk, int nPartitions );

ABC_NAMESPACE_HEADER_END

#endif

////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////