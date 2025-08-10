/**CFile****************************************************************

  FileName    [abcHyperTiming.c]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [Network and node package.]

  Synopsis    [Timing-aware hypergraph construction for AIG network.]

  Author      [Your implementation]
  
  Affiliation [ABC team]

  Date        [Ver. 1.0. Started - January 2025.]

***********************************************************************/

#include "base/abc/abc.h"
#include "map/if/ifHyperAig.h"

ABC_NAMESPACE_IMPL_START

////////////////////////////////////////////////////////////////////////
///                        DECLARATIONS                              ///
////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////
///                     FUNCTION DEFINITIONS                         ///
////////////////////////////////////////////////////////////////////////

/**Function*************************************************************

  Synopsis    [Computes criticality weight for each node based on timing.]

  Description [Returns weight between 1-10, where 10 is most critical.
               Based on node's level relative to max level, and fanout count.]
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
int Aig_ComputeNodeCriticality( Abc_Obj_t * pObj, int maxLevel )
{
    int level, fanoutCount;
    float criticality;
    int weight;
    
    // Get node level
    level = Abc_ObjLevel( pObj );
    
    // Get fanout count (more fanouts = more critical)
    fanoutCount = Abc_ObjFanoutNum( pObj );
    
    // Compute criticality based on level (0 to 1)
    // Nodes at higher levels (closer to outputs) are more critical
    criticality = (float)level / (float)maxLevel;
    
    // Adjust for fanout count
    if ( fanoutCount > 10 )
        criticality *= 1.5;
    else if ( fanoutCount > 5 )
        criticality *= 1.2;
    else if ( fanoutCount > 2 )
        criticality *= 1.1;
    
    // Convert to weight (1-10 scale)
    weight = (int)(criticality * 9) + 1;
    if ( weight > 10 )
        weight = 10;
    if ( weight < 1 )
        weight = 1;
    
    return weight;
}

/**Function*************************************************************

  Synopsis    [Computes edge weight based on timing criticality.]

  Description [Higher weight for edges on critical paths.]
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
int Aig_ComputeEdgeCriticality( Abc_Obj_t * pDriver, Abc_Obj_t * pFanout, int maxLevel )
{
    int driverLevel, fanoutLevel;
    int weight = 1;
    
    driverLevel = Abc_ObjLevel( pDriver );
    fanoutLevel = Abc_ObjLevel( pFanout );
    
    // Check if this edge is on a critical path
    // If fanout level = driver level + 1, it's likely critical
    if ( fanoutLevel == driverLevel + 1 )
    {
        // This edge is on the critical path
        // Give it higher weight based on how close to output
        float criticality = (float)fanoutLevel / (float)maxLevel;
        weight = (int)(criticality * 5) + 1;
        if ( weight > 10 )
            weight = 10;
    }
    
    return weight;
}

/**Function*************************************************************

  Synopsis    [Builds timing-aware hypergraph from AIG network.]

  Description [Similar to Aig_NtkBuildHypergraph but with timing weights.]
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
Aig_Hyper_t * Aig_NtkBuildTimingAwareHypergraph( void * pNtkVoid )
{
    Abc_Ntk_t * pNtk = (Abc_Ntk_t *)pNtkVoid;
    Aig_Hyper_t * pHyper;
    Abc_Obj_t * pObj, * pFanout;
    Vec_Int_t * vConnections;
    int i, j, nodeIdx;
    int maxLevel;

    assert( Abc_NtkIsStrash(pNtk) );

    // Allocate hypergraph structure
    pHyper = Aig_HyperAlloc( pNtk );

    // Compute node levels if not already done
    if ( !Abc_NtkHasMapping(pNtk) )
        Abc_NtkLevel( pNtk );
    
    // Find maximum level for normalization
    maxLevel = Abc_NtkLevel( pNtk );
    if ( maxLevel == 0 )
        maxLevel = 1;

    printf( "Building timing-aware AIG hypergraph...\n" );
    printf( "AIG network: %d PIs, %d POs, %d nodes, max level = %d\n", 
            Abc_NtkPiNum(pNtk), Abc_NtkPoNum(pNtk), Abc_NtkNodeNum(pNtk), maxLevel );

    // Initialize vertex weights based on criticality
    Vec_IntFill( pHyper->vVertexWeights, pHyper->nVertices, 1 );

    // Build hypergraph with timing weights
    Abc_NtkForEachObj( pNtk, pObj, i )
    {
        if ( pObj == NULL )
            continue;

        // Clear connections vector for this node
        vConnections = Vec_IntAlloc( 10 );
        nodeIdx = Abc_ObjId( pObj );

        // Skip constants
        if ( Abc_AigNodeIsConst(pObj) )
        {
            Vec_IntFree( vConnections );
            continue;
        }

        // Set vertex weight based on criticality
        if ( nodeIdx < Vec_IntSize(pHyper->vVertexWeights) )
        {
            int nodeWeight = Aig_ComputeNodeCriticality( pObj, maxLevel );
            Vec_IntWriteEntry( pHyper->vVertexWeights, nodeIdx, nodeWeight );
        }

        // Compute edge weight based on criticality of connections
        int edgeWeight = 1;
        
        if ( !Abc_ObjIsPo(pObj) )
        {
            // For non-PO nodes: collect fanouts
            int maxEdgeWeight = 1;
            Abc_ObjForEachFanout( pObj, pFanout, j )
            {
                // Only include AND nodes and PO nodes as fanouts
                if ( Abc_ObjIsNode(pFanout) || Abc_ObjIsPo(pFanout) )
                {
                    Vec_IntPush( vConnections, Abc_ObjId(pFanout) );
                    
                    // Compute edge criticality
                    int w = Aig_ComputeEdgeCriticality( pObj, pFanout, maxLevel );
                    if ( w > maxEdgeWeight )
                        maxEdgeWeight = w;
                }
            }
            edgeWeight = maxEdgeWeight;
        }
        else if ( Abc_ObjIsPo(pObj) && !Abc_ObjIsLatch(pObj) )
        {
            // For PO nodes (non-latch): collect fanins
            if ( Abc_ObjFanin0(pObj) && !Abc_AigNodeIsConst(Abc_ObjFanin0(pObj)) )
            {
                Vec_IntPush( vConnections, Abc_ObjId(Abc_ObjFanin0(pObj)) );
                
                // PO connections are always critical
                edgeWeight = 10;
            }
        }

        // Create hyperedge if connections exist
        if ( Vec_IntSize(vConnections) > 0 )
        {
            Vec_Int_t * vHyperEdge = Vec_IntAlloc( Vec_IntSize(vConnections) + 1 );
            
            // Add root node at the beginning
            Vec_IntPush( vHyperEdge, nodeIdx );
            
            // Add all connections
            int connId;
            Vec_IntForEachEntry( vConnections, connId, j )
                Vec_IntPush( vHyperEdge, connId );

            // Add to hypergraph with computed weight
            Vec_PtrPush( (Vec_Ptr_t *)pHyper->vHyperedges, vHyperEdge );
            Vec_IntPush( pHyper->vEdgeWeights, edgeWeight );
            pHyper->nHyperedges++;
            pHyper->nPins += Vec_IntSize(vHyperEdge);
        }

        Vec_IntFree( vConnections );
    }

    // Print statistics
    printf( "Timing-aware hypergraph construction completed:\n" );
    printf( "  %d hyperedges, %d pins\n", pHyper->nHyperedges, pHyper->nPins );
    
    // Print weight distribution
    int weightHist[11] = {0};
    int weight;
    Vec_IntForEachEntry( pHyper->vVertexWeights, weight, i )
    {
        if ( weight >= 0 && weight <= 10 )
            weightHist[weight]++;
    }
    printf( "  Vertex weight distribution:\n" );
    for ( i = 1; i <= 10; i++ )
    {
        if ( weightHist[i] > 0 )
            printf( "    Weight %2d: %d vertices\n", i, weightHist[i] );
    }

    return pHyper;
}

/**Function*************************************************************

  Synopsis    [Test function for timing-aware hypergraph.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
int Aig_TestTimingAwareHypergraph( void * pNtk )
{
    Aig_Hyper_t * pHyper;
    
    // Build timing-aware hypergraph
    pHyper = Aig_NtkBuildTimingAwareHypergraph( pNtk );
    if ( pHyper == NULL )
    {
        printf( "Failed to build timing-aware hypergraph\n" );
        return 0;
    }
    
    // Print statistics
    Aig_HyperPrintStats( pHyper );
    
    // Clean up
    Aig_HyperFree( pHyper );
    
    return 1;
}

////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////

ABC_NAMESPACE_IMPL_END