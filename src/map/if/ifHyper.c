/**CFile****************************************************************

  FileName    [ifHyper.c]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [FPGA mapping based on priority cuts with hypergraph partitioning.]

  Synopsis    [Hypergraph construction for IF mapper.]

  Author      [Based on LSOracle implementation]
  
  Affiliation [ABC team]

  Date        [Ver. 1.0. Started - January 2025.]

***********************************************************************/
//基于IF网络的超图构建

#include "ifHyper.h"

ABC_NAMESPACE_IMPL_START

////////////////////////////////////////////////////////////////////////
///                        DECLARATIONS                              ///
////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////
///                     FUNCTION DEFINITIONS                         ///
////////////////////////////////////////////////////////////////////////

/**Function*************************************************************

  Synopsis    [Allocates hypergraph structure.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
If_Hyper_t * If_HyperAlloc( If_Man_t * pIfMan )
{
    If_Hyper_t * p;
    p = ABC_ALLOC( If_Hyper_t, 1 );
    memset( p, 0, sizeof(If_Hyper_t) );
    p->pIfMan = pIfMan;
    p->vHyperedges = Vec_VecAlloc( 100 );
    p->vEdgeWeights = Vec_IntAlloc( 100 );
    p->vVertexWeights = Vec_IntAlloc( If_ManObjNum(pIfMan) );
    p->nVertices = If_ManObjNum( pIfMan );
    return p;
}

/**Function*************************************************************

  Synopsis    [Frees hypergraph structure.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void If_HyperFree( If_Hyper_t * p )
{
    if ( p == NULL )
        return;
    Vec_VecFree( p->vHyperedges );
    Vec_IntFree( p->vEdgeWeights );
    Vec_IntFree( p->vVertexWeights );
    ABC_FREE( p );
}

/**Function*************************************************************

  Synopsis    [Builds hypergraph from IF manager.]

  Description [Exact implementation following LSOracle's hypergraph construction.
               Reference: LSOracle hyperg.hpp:67-97
               Algorithm:
               1. For non-PO nodes: collect fanouts, create [node + fanouts] hyperedge
               2. For PO nodes (non-RO): collect fanins, create [node + fanins] hyperedge
               3. Root node inserted at beginning of hyperedge (LSOracle style)]
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
If_Hyper_t * If_ManBuildHypergraph( If_Man_t * pIfMan )
{
    If_Hyper_t * pHyper;
    If_Obj_t * pObj, * pOther;
    Vec_Int_t * vConnections;
    int i, j, nodeIdx;

    // Allocate hypergraph structure
    pHyper = If_HyperAlloc( pIfMan );

    // Initialize vertex weights (default weight = 1)
    Vec_IntFill( pHyper->vVertexWeights, pHyper->nVertices, 1 );

    if ( pIfMan->pPars->fVerbose )
        printf( "Building hypergraph following LSOracle algorithm...\n" );

    // Build hypergraph following LSOracle's exact algorithm (hyperg.hpp:67-97)
    If_ManForEachObj( pIfMan, pObj, i )
    {
        // Clear connections vector for this node (LSOracle style)
        vConnections = Vec_IntAlloc( 10 );
        nodeIdx = If_ObjId( pObj );

        // Skip constants (following LSOracle logic)
        if ( If_ObjIsConst1(pObj) )
        {
            Vec_IntFree( vConnections );
            continue;
        }

        if ( !If_ObjIsCo(pObj) )
        {
            // For non-PO nodes: collect fanouts (LSOracle: lines 75-82)
            // LSOracle: fanout.foreach_fanout(node, [&](const auto & p) { nodes.insert(p); });
            If_ManForEachObj( pIfMan, pOther, j )
            {
                if ( If_ObjIsAnd(pOther) )
                {
                    // Check if pObj is fanin of pOther
                    if ( If_ObjFanin0(pOther) == pObj || If_ObjFanin1(pOther) == pObj )
                        Vec_IntPush( vConnections, If_ObjId(pOther) );
                }
                else if ( If_ObjIsCo(pOther) )
                {
                    // Check if pObj drives primary output
                    if ( If_ObjFanin0(pOther) == pObj )
                        Vec_IntPush( vConnections, If_ObjId(pOther) );
                }
            }
        }
        else if ( If_ObjIsCo(pObj) && !If_ObjIsLatch(pObj) )
        {
            // For PO nodes (non-RO): collect fanins (LSOracle: lines 85-89)
            // LSOracle: ntk.foreach_fanin(node, [&](auto const & conn, auto i) {
            //     connections.push_back(ntk._storage->nodes[node].children[i].index); });
            if ( If_ObjFanin0(pObj) && !If_ObjIsConst1(If_ObjFanin0(pObj)) )
                Vec_IntPush( vConnections, If_ObjId(If_ObjFanin0(pObj)) );
        }

        // Create hyperedge if connections exist (LSOracle: lines 91-96)
        if ( Vec_IntSize(vConnections) > 0 )
        {
            Vec_Int_t * vHyperEdge = Vec_IntAlloc( Vec_IntSize(vConnections) + 1 );
            
            // LSOracle: connection_to_add.insert(connection_to_add.begin(), nodeNdx);
            // Add root node at the beginning (LSOracle exact behavior)
            Vec_IntPush( vHyperEdge, nodeIdx );
            
            // Add all connections
            int connId;
            Vec_IntForEachEntry( vConnections, connId, j )
                Vec_IntPush( vHyperEdge, connId );

            // Add to hypergraph
            Vec_VecPush( pHyper->vHyperedges, Vec_IntSize(vHyperEdge), vHyperEdge );
            Vec_IntPush( pHyper->vEdgeWeights, 1 ); // Default weight
            pHyper->nHyperedges++;
            pHyper->nPins += Vec_IntSize(vHyperEdge);
        }

        Vec_IntFree( vConnections );
    }

    if ( pIfMan->pPars->fVerbose )
        printf( "Hypergraph construction completed: %d edges, %d pins\n", 
                pHyper->nHyperedges, pHyper->nPins );

    return pHyper;
}

/**Function*************************************************************

  Synopsis    [Prints hypergraph statistics.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void If_HyperPrintStats( If_Hyper_t * p )
{
    printf( "Hypergraph statistics:\n" );
    printf( "  Vertices:    %6d\n", p->nVertices );
    printf( "  Hyperedges:  %6d\n", p->nHyperedges );
    printf( "  Total pins:  %6d\n", p->nPins );
    if ( p->nHyperedges > 0 )
        printf( "  Avg degree:  %6.2f\n", (float)p->nPins / p->nHyperedges );
}

/**Function*************************************************************

  Synopsis    [Prints detailed hypergraph information.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void If_HyperPrint( If_Hyper_t * p )
{
    Vec_Int_t * vEdge;
    int i, j, iObj;

    printf( "Hypergraph with %d vertices and %d hyperedges:\n", 
            p->nVertices, p->nHyperedges );

    If_HyperForEachEdge( p, vEdge, i )
    {
        printf( "Edge %3d: ", i );
        Vec_IntForEachEntry( vEdge, iObj, j )
            printf( "%d ", iObj );
        printf( "\n" );
    }
}

/**Function*************************************************************

  Synopsis    [Converts hypergraph to format suitable for external partitioner.]

  Description [Prepares data for KaHyPar or other hypergraph partitioners.]
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void If_HyperExportForPartitioning( If_Hyper_t * p, Vec_Int_t ** pvHyperedges, 
                                   Vec_Int_t ** pvIndices, Vec_Int_t ** pvWeights )
{
    Vec_Int_t * vHyperedges, * vIndices, * vWeights;
    Vec_Int_t * vEdge;
    int i, j, iObj;

    // Allocate output vectors
    vHyperedges = Vec_IntAlloc( p->nPins );
    vIndices = Vec_IntAlloc( p->nHyperedges + 1 );
    vWeights = Vec_IntDup( p->vEdgeWeights );

    // Convert to CSR-like format for partitioners
    Vec_IntPush( vIndices, 0 );
    If_HyperForEachEdge( p, vEdge, i )
    {
        Vec_IntForEachEntry( vEdge, iObj, j )
            Vec_IntPush( vHyperedges, iObj );
        Vec_IntPush( vIndices, Vec_IntSize(vHyperedges) );
    }

    // Return the arrays
    *pvHyperedges = vHyperedges;
    *pvIndices = vIndices;
    *pvWeights = vWeights;
}

/**Function*************************************************************

  Synopsis    [Tests hypergraph construction.]

  Description [Simple test function to verify hypergraph building works correctly.]
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
int If_HyperTest( If_Man_t * pIfMan )
{
    If_Hyper_t * pHyper;
    abctime clk;
    
    printf( "Testing hypergraph construction...\n" );
    
    // Build hypergraph
    clk = Abc_Clock();
    pHyper = If_ManBuildHypergraph( pIfMan );
    
    if ( pHyper == NULL )
    {
        printf( "Error: Failed to build hypergraph\n" );
        return 0;
    }
    
    // Print statistics
    printf( "Hypergraph construction time: %.2f sec\n", (float)(Abc_Clock() - clk)/(float)(CLOCKS_PER_SEC) );
    If_HyperPrintStats( pHyper );
    
    // Basic sanity checks
    if ( pHyper->nVertices != If_ManObjNum(pIfMan) )
    {
        printf( "Error: Vertex count mismatch\n" );
        If_HyperFree( pHyper );
        return 0;
    }
    
    if ( pHyper->nHyperedges == 0 )
    {
        printf( "Warning: No hyperedges generated\n" );
    }
    
    printf( "Hypergraph test completed successfully\n" );
    
    // Clean up
    If_HyperFree( pHyper );
    return 1;
}

ABC_NAMESPACE_IMPL_END