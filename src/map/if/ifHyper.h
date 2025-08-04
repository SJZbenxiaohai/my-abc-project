/**CFile****************************************************************

  FileName    [ifHyper.h]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [FPGA mapping based on priority cuts with hypergraph partitioning.]

  Synopsis    [External declarations for hypergraph construction.]

  Author      [Based on LSOracle implementation]
  
  Affiliation [ABC team]

  Date        [Ver. 1.0. Started - January 2025.]

***********************************************************************/

#ifndef ABC__map__if__ifHyper_h
#define ABC__map__if__ifHyper_h

////////////////////////////////////////////////////////////////////////
///                          INCLUDES                                ///
////////////////////////////////////////////////////////////////////////

#include "if.h"

ABC_NAMESPACE_HEADER_START

////////////////////////////////////////////////////////////////////////
///                         PARAMETERS                               ///
////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////
///                         BASIC TYPES                              ///
////////////////////////////////////////////////////////////////////////

// Hypergraph data structure (based on LSOracle implementation)
typedef struct If_Hyper_t_ If_Hyper_t;
struct If_Hyper_t_
{
    int                nVertices;      // number of vertices (IF objects)
    int                nHyperedges;    // number of hyperedges
    int                nPins;          // total number of pins (connections)
    Vec_Vec_t *        vHyperedges;    // hyperedges (each contains vertex IDs)
    Vec_Int_t *        vEdgeWeights;   // weights of hyperedges
    Vec_Int_t *        vVertexWeights; // weights of vertices
    If_Man_t *         pIfMan;         // pointer to IF manager
};

////////////////////////////////////////////////////////////////////////
///                      MACRO DEFINITIONS                           ///
////////////////////////////////////////////////////////////////////////

#define If_HyperForEachEdge( p, vEdge, i )                  \
    Vec_VecForEachLevel( (p)->vHyperedges, vEdge, i )

    
////////////////////////////////////////////////////////////////////////
///                    FUNCTION DECLARATIONS                         ///
////////////////////////////////////////////////////////////////////////

/*=== ifHyper.c ==========================================================*/
extern If_Hyper_t *    If_HyperAlloc( If_Man_t * pIfMan );
extern void            If_HyperFree( If_Hyper_t * p );
extern If_Hyper_t *    If_ManBuildHypergraph( If_Man_t * pIfMan );
extern void            If_HyperPrint( If_Hyper_t * p );
extern void            If_HyperPrintStats( If_Hyper_t * p );
extern void            If_HyperExportForPartitioning( If_Hyper_t * p, Vec_Int_t ** pvHyperedges, Vec_Int_t ** pvIndices, Vec_Int_t ** pvWeights );
extern int             If_HyperTest( If_Man_t * pIfMan );

ABC_NAMESPACE_HEADER_END

#endif

////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////