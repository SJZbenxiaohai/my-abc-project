/**CFile****************************************************************

  FileName    [ifHyperAig.h]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [FPGA mapping based on priority cuts with hypergraph partitioning.]

  Synopsis    [Hypergraph construction directly from AIG network.]

  Author      [Based on LSOracle implementation]
  
  Affiliation [ABC team]

  Date        [Ver. 1.0. Started - January 2025.]

***********************************************************************/

#ifndef ABC__map__if__ifHyperAig_h
#define ABC__map__if__ifHyperAig_h

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

typedef struct Aig_Hyper_t_ Aig_Hyper_t;

// Hypergraph structure for AIG networks
struct Aig_Hyper_t_
{
    void *                 pNtk;          // the AIG network (void* to avoid dependency)
    int                    nVertices;     // number of vertices (AIG nodes)
    int                    nHyperedges;   // number of hyperedges
    int                    nPins;         // total number of pins
    Vec_Vec_t *            vHyperedges;   // hyperedges (vector of vectors)
    Vec_Int_t *            vEdgeWeights;  // weights of hyperedges
    Vec_Int_t *            vVertexWeights;// weights of vertices
};

////////////////////////////////////////////////////////////////////////
///                      MACRO DEFINITIONS                           ///
////////////////////////////////////////////////////////////////////////

#define Aig_HyperForEachEdge( p, vEdge, i )                                    \
    for ( i = 0; (i < Vec_VecSize(p->vHyperedges)) && (((vEdge) = (Vec_Int_t *)Vec_VecEntry(p->vHyperedges, i)), 1); i++ )

////////////////////////////////////////////////////////////////////////
///                    FUNCTION DECLARATIONS                         ///
////////////////////////////////////////////////////////////////////////

/*=== ifHyperAig.c ==========================================================*/
extern Aig_Hyper_t *       Aig_HyperAlloc( void * pNtk );
extern void                Aig_HyperFree( Aig_Hyper_t * p );
extern Aig_Hyper_t *       Aig_NtkBuildHypergraph( void * pNtk );
extern void                Aig_HyperPrintStats( Aig_Hyper_t * p );
extern void                Aig_HyperPrint( Aig_Hyper_t * p );
extern void                Aig_HyperExportForPartitioning( Aig_Hyper_t * p, Vec_Int_t ** pvHyperedges, 
                                                           Vec_Int_t ** pvIndices, Vec_Int_t ** pvWeights );
extern int                 Aig_HyperTest( void * pNtk );

ABC_NAMESPACE_HEADER_END

#endif

////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////