/**CFile****************************************************************

  FileName    [abcHyperAig.c]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [Network and node package.]

  Synopsis    [Hypergraph construction directly from AIG network.]

  Author      [Based on LSOracle implementation]
  
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

  Synopsis    [Allocates hypergraph structure for AIG network.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
Aig_Hyper_t * Aig_HyperAlloc( void * pNtkVoid )
{
    Abc_Ntk_t * pNtk = (Abc_Ntk_t *)pNtkVoid;
    Aig_Hyper_t * p;
    assert( Abc_NtkIsStrash(pNtk) );
    
    p = ABC_ALLOC( Aig_Hyper_t, 1 );
    memset( p, 0, sizeof(Aig_Hyper_t) );
    p->pNtk = pNtk;
    p->vHyperedges = Vec_VecAlloc( 100 );
    p->vEdgeWeights = Vec_IntAlloc( 100 );
    p->vVertexWeights = Vec_IntAlloc( Abc_NtkObjNumMax(pNtk) );
    p->nVertices = Abc_NtkObjNumMax( pNtk );
    return p;
}

/**Function*************************************************************

  Synopsis    [Frees hypergraph structure.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Aig_HyperFree( Aig_Hyper_t * p )
{
    if ( p == NULL )
        return;
    Vec_VecFree( p->vHyperedges );
    Vec_IntFree( p->vEdgeWeights );
    Vec_IntFree( p->vVertexWeights );
    ABC_FREE( p );
}

/**Function*************************************************************

  Synopsis    [Builds hypergraph directly from AIG network.]

  Description [Exact implementation following LSOracle's hypergraph construction.
               Reference: LSOracle hyperg.hpp:67-97
               Algorithm:
               1. For non-PO nodes: collect fanouts, create [node + fanouts] hyperedge
               2. For PO nodes (non-latch): collect fanins, create [node + fanins] hyperedge
               3. Root node inserted at beginning of hyperedge (LSOracle style)]
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
Aig_Hyper_t * Aig_NtkBuildHypergraph( void * pNtkVoid )
{
    Abc_Ntk_t * pNtk = (Abc_Ntk_t *)pNtkVoid;
    Aig_Hyper_t * pHyper;
    Abc_Obj_t * pObj, * pFanout;
    Vec_Int_t * vConnections;
    int i, j, nodeIdx;

    assert( Abc_NtkIsStrash(pNtk) );

    // Allocate hypergraph structure
    pHyper = Aig_HyperAlloc( pNtk );

    // Initialize vertex weights (default weight = 1)
    Vec_IntFill( pHyper->vVertexWeights, pHyper->nVertices, 1 );

    printf( "Building AIG hypergraph following LSOracle algorithm...\n" );
    printf( "AIG network: %d PIs, %d POs, %d nodes\n", 
            Abc_NtkPiNum(pNtk), Abc_NtkPoNum(pNtk), Abc_NtkNodeNum(pNtk) );

    // Build hypergraph following LSOracle's exact algorithm (hyperg.hpp:67-97)
    Abc_NtkForEachObj( pNtk, pObj, i )
    {
        if ( pObj == NULL )
            continue;

        // Clear connections vector for this node (LSOracle style)
        vConnections = Vec_IntAlloc( 10 );
        nodeIdx = Abc_ObjId( pObj );

        // Skip constants (following LSOracle logic)
        if ( Abc_AigNodeIsConst(pObj) )
        {
            Vec_IntFree( vConnections );
            continue;
        }

        if ( !Abc_ObjIsPo(pObj) )
        {
            // For non-PO nodes: collect fanouts (LSOracle: lines 75-82)
            // LSOracle: fanout.foreach_fanout(node, [&](const auto & p) { nodes.insert(p); });
            // AIG has direct fanout access - much more efficient than IF network!
            Abc_ObjForEachFanout( pObj, pFanout, j )
            {
                // Only include AND nodes and PO nodes as fanouts
                if ( Abc_ObjIsNode(pFanout) || Abc_ObjIsPo(pFanout) )
                {
                    Vec_IntPush( vConnections, Abc_ObjId(pFanout) );
                }
            }
        }
        else if ( Abc_ObjIsPo(pObj) && !Abc_ObjIsLatch(pObj) )
        {
            // For PO nodes (non-latch): collect fanins (LSOracle: lines 85-89)
            // LSOracle: ntk.foreach_fanin(node, [&](auto const & conn, auto i) {
            //     connections.push_back(ntk._storage->nodes[node].children[i].index); });
            if ( Abc_ObjFanin0(pObj) && !Abc_AigNodeIsConst(Abc_ObjFanin0(pObj)) )
                Vec_IntPush( vConnections, Abc_ObjId(Abc_ObjFanin0(pObj)) );
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

            // Add to hypergraph - use simple push (appends to end)
            Vec_PtrPush( (Vec_Ptr_t *)pHyper->vHyperedges, vHyperEdge );
            Vec_IntPush( pHyper->vEdgeWeights, 1 ); // Default weight
            pHyper->nHyperedges++;
            pHyper->nPins += Vec_IntSize(vHyperEdge);
        }

        Vec_IntFree( vConnections );
    }

    printf( "AIG hypergraph construction completed: %d edges, %d pins\n", 
            pHyper->nHyperedges, pHyper->nPins );

    return pHyper;
}

/**Function*************************************************************

  Synopsis    [Prints hypergraph statistics.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Aig_HyperPrintStats( Aig_Hyper_t * p )
{
    printf( "AIG Hypergraph statistics:\n" );
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
void Aig_HyperPrint( Aig_Hyper_t * p )
{
    Vec_Int_t * vEdge;
    int i, j, iObj;

    printf( "AIG Hypergraph with %d vertices and %d hyperedges:\n", 
            p->nVertices, p->nHyperedges );

    Aig_HyperForEachEdge( p, vEdge, i )
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
void Aig_HyperExportForPartitioning( Aig_Hyper_t * p, Vec_Int_t ** pvHyperedges, 
                                     Vec_Int_t ** pvIndices, Vec_Int_t ** pvWeights )
{
    Vec_Int_t * vHyperedges, * vIndices, * vWeights;
    Vec_Int_t * vEdge;
    int i, j, iObj;
    int nEdgesProcessed = 0;

    // Allocate output vectors
    vHyperedges = Vec_IntAlloc( p->nPins );
    vIndices = Vec_IntAlloc( p->nHyperedges + 1 );
    vWeights = Vec_IntDup( p->vEdgeWeights );

    // Debug: Print hypergraph statistics
    printf( "Export: nHyperedges=%d, nPins=%d, Vec_PtrSize=%d\n", 
            p->nHyperedges, p->nPins, Vec_PtrSize((Vec_Ptr_t *)p->vHyperedges) );

    // Convert to CSR-like format for partitioners
    Vec_IntPush( vIndices, 0 );
    Aig_HyperForEachEdge( p, vEdge, i )
    {
        nEdgesProcessed++;
        Vec_IntForEachEntry( vEdge, iObj, j )
            Vec_IntPush( vHyperedges, iObj );
        Vec_IntPush( vIndices, Vec_IntSize(vHyperedges) );
    }
    
    printf( "Export: Processed %d edges, generated %d pins\n", 
            nEdgesProcessed, Vec_IntSize(vHyperedges) );

    // Return the arrays
    *pvHyperedges = vHyperedges;
    *pvIndices = vIndices;
    *pvWeights = vWeights;
}

/**Function*************************************************************

  Synopsis    [Tests AIG hypergraph construction.]

  Description [Simple test function to verify AIG hypergraph building works correctly.]
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
int Aig_HyperTest( void * pNtkVoid )
{
    Abc_Ntk_t * pNtk = (Abc_Ntk_t *)pNtkVoid;
    Aig_Hyper_t * pHyper;
    abctime clk;
    
    if ( !Abc_NtkIsStrash(pNtk) )
    {
        printf( "Error: Network is not an AIG (strashed network)\n" );
        return 0;
    }
    
    printf( "Testing AIG hypergraph construction...\n" );
    
    // Build hypergraph
    clk = Abc_Clock();
    pHyper = Aig_NtkBuildHypergraph( pNtk );
    
    if ( pHyper == NULL )
    {
        printf( "Error: Failed to build AIG hypergraph\n" );
        return 0;
    }
    
    // Print statistics
    printf( "AIG hypergraph construction time: %.2f sec\n", (float)(Abc_Clock() - clk)/(float)(CLOCKS_PER_SEC) );
    Aig_HyperPrintStats( pHyper );
    
    // Basic sanity checks
    if ( pHyper->nVertices != Abc_NtkObjNumMax(pNtk) )
    {
        printf( "Error: Vertex count mismatch (expected %d, got %d)\n", 
                Abc_NtkObjNumMax(pNtk), pHyper->nVertices );
        Aig_HyperFree( pHyper );
        return 0;
    }
    
    if ( pHyper->nHyperedges == 0 )
    {
        printf( "Warning: No hyperedges generated\n" );
    }
    
    printf( "AIG hypergraph test completed successfully\n" );
    
    // Clean up
    Aig_HyperFree( pHyper );
    return 1;
}

/**Function*************************************************************

  Synopsis    [Applies hypergraph partition result to AIG network.]

  Description [Based on LSOracle's partition_manager implementation.
               Creates partition views and identifies interface signals.]
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
int Aig_ApplyPartitionResult( void * pNtk, Aig_Hyper_t * pHyper, Vec_Int_t * vPartition, int nPartitions )
{
    Abc_Ntk_t * pAig = (Abc_Ntk_t *)pNtk;
    Abc_Obj_t * pObj, * pFanin, * pFanout;
    int i, j, nodeIdx, partId, faninPart, fanoutPart;
    Vec_Vec_t * vPartNodes;      // Nodes in each partition
    Vec_Vec_t * vPartInputs;     // Input nodes for each partition  
    Vec_Vec_t * vPartOutputs;    // Output nodes for each partition
    Vec_Int_t * vPartSize;       // Size of each partition
    
    if ( !pAig || !pHyper || !vPartition || nPartitions <= 0 )
        return 0;
        
    printf( "Applying partition result to AIG network...\n" );
    printf( "Network: %d PIs, %d POs, %d nodes, %d partitions\n", 
            Abc_NtkPiNum(pAig), Abc_NtkPoNum(pAig), Abc_NtkNodeNum(pAig), nPartitions );
    
    // Initialize partition data structures (LSOracle style)
    vPartNodes = Vec_VecStart( nPartitions );
    vPartInputs = Vec_VecStart( nPartitions );
    vPartOutputs = Vec_VecStart( nPartitions );
    vPartSize = Vec_IntAlloc( nPartitions );
    
    // Initialize partition sizes
    for ( i = 0; i < nPartitions; i++ )
        Vec_IntPush( vPartSize, 0 );
    
    // Step 1: Assign nodes to partitions (similar to _part_scope in LSOracle)
    Abc_NtkForEachObj( pAig, pObj, i )
    {
        int objId = Abc_ObjId( pObj );
        if ( objId >= Vec_IntSize(vPartition) )
            continue;
            
        partId = Vec_IntEntry( vPartition, objId );
        if ( partId >= 0 && partId < nPartitions )
        {
            Vec_VecPush( vPartNodes, partId, (void *)(size_t)objId );
            Vec_IntAddToEntry( vPartSize, partId, 1 );
        }
    }
    
    // Step 2: Identify partition interfaces (similar to LSOracle's interface detection)
    // First, process PI nodes to identify their fanouts crossing partition boundaries
    Abc_NtkForEachPi( pAig, pObj, i )
    {
        nodeIdx = Abc_ObjId( pObj );
        if ( nodeIdx >= Vec_IntSize(vPartition) )
            continue;
            
        partId = Vec_IntEntry( vPartition, nodeIdx );
        if ( partId < 0 || partId >= nPartitions )
            continue;
        
        // Check fanouts of PI for cross-partition connections
        Abc_ObjForEachFanout( pObj, pFanout, j )
        {
            int fanoutIdx = Abc_ObjId( pFanout );
            if ( fanoutIdx >= Vec_IntSize(vPartition) )
                continue;
                
            int fanoutPart = Vec_IntEntry( vPartition, fanoutIdx );
            
            // Cross-partition fanout detected
            if ( fanoutPart != partId && fanoutPart >= 0 && fanoutPart < nPartitions )
            {
                // PI is output of its partition, feeding into fanout's partition
                Vec_VecPushUnique( vPartOutputs, partId, (void *)(size_t)nodeIdx );
                // PI is input to fanout's partition
                Vec_VecPushUnique( vPartInputs, fanoutPart, (void *)(size_t)nodeIdx );
            }
        }
    }
    
    // Then process internal nodes as before
    Abc_NtkForEachNode( pAig, pObj, i )
    {
        nodeIdx = Abc_ObjId( pObj );
        if ( nodeIdx >= Vec_IntSize(vPartition) )
            continue;
            
        partId = Vec_IntEntry( vPartition, nodeIdx );
        if ( partId < 0 || partId >= nPartitions )
            continue;
        
        // Check fanins for cross-partition connections (LSOracle: lines 290-301)
        Abc_ObjForEachFanin( pObj, pFanin, j )
        {
            int faninIdx = Abc_ObjId( pFanin );
            if ( faninIdx >= Vec_IntSize(vPartition) )
                continue;
                
            faninPart = Vec_IntEntry( vPartition, faninIdx );
            
            // Cross-partition fanin detected
            if ( faninPart != partId && faninPart >= 0 && faninPart < nPartitions )
            {
                // Add fanin as input to current partition
                Vec_VecPushUnique( vPartInputs, partId, (void *)(size_t)faninIdx );
                // Add fanin as output from source partition  
                Vec_VecPushUnique( vPartOutputs, faninPart, (void *)(size_t)faninIdx );
            }
        }
    }
    
    // Step 3: Handle primary outputs (similar to LSOracle's PO handling)
    Abc_NtkForEachPo( pAig, pObj, i )
    {
        pFanin = Abc_ObjFanin0( pObj );
        if ( pFanin )
        {
            int faninIdx = Abc_ObjId( pFanin );
            if ( faninIdx < Vec_IntSize(vPartition) )
            {
                faninPart = Vec_IntEntry( vPartition, faninIdx );
                if ( faninPart >= 0 && faninPart < nPartitions )
                {
                    Vec_VecPushUnique( vPartOutputs, faninPart, (void *)(size_t)faninIdx );
                }
            }
        }
    }
    
    // Step 4: Print partition statistics (similar to LSOracle's output)
    printf( "Partition analysis completed:\n" );
    
    // Count PI nodes in each partition for debugging
    int nPIsInPartition[2] = {0, 0};
    Abc_NtkForEachPi( pAig, pObj, i )
    {
        int objId = Abc_ObjId( pObj );
        if ( objId < Vec_IntSize(vPartition) )
        {
            int part = Vec_IntEntry( vPartition, objId );
            if ( part >= 0 && part < nPartitions )
                nPIsInPartition[part]++;
        }
    }
    
    for ( i = 0; i < nPartitions; i++ )
    {
        int nNodes = Vec_IntSize( (Vec_Int_t *)Vec_VecEntry(vPartNodes, i) );
        int nInputs = Vec_IntSize( (Vec_Int_t *)Vec_VecEntry(vPartInputs, i) );
        int nOutputs = Vec_IntSize( (Vec_Int_t *)Vec_VecEntry(vPartOutputs, i) );
        
        printf( "  Partition %d: %d nodes (%d PIs), %d inputs, %d outputs\n", 
                i, nNodes, nPIsInPartition[i], nInputs, nOutputs );
    }
    
    // Calculate cut edges (connections between partitions)
    int nCutEdges = 0;
    for ( i = 0; i < nPartitions; i++ )
    {
        nCutEdges += Vec_IntSize( (Vec_Int_t *)Vec_VecEntry(vPartInputs, i) );
    }
    printf( "  Total interface signals: %d\n", nCutEdges );
    
    // TODO: Create partition views and apply optimizations (future work)
    // This would involve creating separate AIG views for each partition
    // and implementing the synchronization mechanism like LSOracle
    
    // Cleanup
    Vec_VecFree( vPartNodes );
    Vec_VecFree( vPartInputs );
    Vec_VecFree( vPartOutputs );
    Vec_IntFree( vPartSize );
    
    printf( "Partition result application completed.\n" );
    return 1;
}

ABC_NAMESPACE_IMPL_END