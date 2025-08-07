/**CFile****************************************************************

  FileName    [ifPartition.c]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [FPGA mapping based on priority cuts.]

  Synopsis    [Partition-aware mapping functions.]

  Author      [Your name]
  
  Affiliation [UC Berkeley]

  Date        [Ver. 1.0. Started - January 2025.]

  Revision    [$Id: ifPartition.c,v 1.00 2025/01/01 00:00:00 alanmi Exp $]

***********************************************************************/

#include "if.h"
#include "base/abc/abc.h"

ABC_NAMESPACE_IMPL_START

////////////////////////////////////////////////////////////////////////
///                        DECLARATIONS                              ///
////////////////////////////////////////////////////////////////////////

/**Function*************************************************************

  Synopsis    [Counts the number of 1s in the signature.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
static inline int If_WordCountOnes( unsigned uWord )
{
    uWord = (uWord & 0x55555555) + ((uWord>>1) & 0x55555555);
    uWord = (uWord & 0x33333333) + ((uWord>>2) & 0x33333333);
    uWord = (uWord & 0x0F0F0F0F) + ((uWord>>4) & 0x0F0F0F0F);
    uWord = (uWord & 0x00FF00FF) + ((uWord>>8) & 0x00FF00FF);
    return  (uWord & 0x0000FFFF) + (uWord>>16);
}

////////////////////////////////////////////////////////////////////////
///                     FUNCTION DEFINITIONS                         ///
////////////////////////////////////////////////////////////////////////

/**Function*************************************************************

  Synopsis    [Sets partition information in IF manager.]

  Description [Transfers partition data from AIG to IF network.]
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void If_ManSetPartitionInfo( If_Man_t * pIfMan, void * pNtk, Vec_Int_t * vPartition, int nPartitions )
{
    Abc_Ntk_t * pAig = (Abc_Ntk_t *)pNtk;
    Abc_Obj_t * pObj;
    If_Obj_t * pIfObj;
    int i, nodeId, partId;
    
    if ( !pIfMan || !vPartition )
        return;
    
    printf( "Setting partition information in IF manager...\n" );
    
    // Create a new partition vector for IF objects
    // IF object IDs may differ from AIG node IDs
    pIfMan->vPartition = Vec_IntStart( If_ManObjNum(pIfMan) );
    pIfMan->nPartitions = nPartitions;
    
    // Initialize all partitions to -1 (unassigned)
    Vec_IntFill( pIfMan->vPartition, If_ManObjNum(pIfMan), -1 );
    
    // Initialize partition input/output tracking
    pIfMan->vPartInputs = Vec_VecStart( nPartitions );
    pIfMan->vPartOutputs = Vec_VecStart( nPartitions );
    
    // Map AIG node IDs to IF object IDs and identify partition boundaries
    Abc_NtkForEachNode( pAig, pObj, i )
    {
        pIfObj = (If_Obj_t *)pObj->pCopy;
        if ( !pIfObj )
            continue;
            
        nodeId = Abc_ObjId( pObj );
        if ( nodeId >= Vec_IntSize(vPartition) )
            continue;
            
        partId = Vec_IntEntry( vPartition, nodeId );
        
        // Set the partition for the IF object using IF object ID
        Vec_IntWriteEntry( pIfMan->vPartition, If_ObjId(pIfObj), partId );
        
        // Check fanins for cross-partition connections
        if ( Abc_ObjFaninNum(pObj) > 0 )
        {
            Abc_Obj_t * pFanin;
            int j, faninId, faninPart;
            
            Abc_ObjForEachFanin( pObj, pFanin, j )
            {
                faninId = Abc_ObjId( pFanin );
                if ( faninId >= Vec_IntSize(vPartition) )
                    continue;
                    
                faninPart = Vec_IntEntry( vPartition, faninId );
                
                // Cross-partition edge detected
                if ( faninPart != partId && faninPart >= 0 && partId >= 0 )
                {
                    // Mark fanin as output of its partition
                    Vec_VecPushUniqueInt( pIfMan->vPartOutputs, faninPart, If_ObjId((If_Obj_t *)pFanin->pCopy) );
                    // Mark fanin as input to current partition
                    Vec_VecPushUniqueInt( pIfMan->vPartInputs, partId, If_ObjId((If_Obj_t *)pFanin->pCopy) );
                }
            }
        }
    }
    
    // Print partition statistics
    printf( "Partition boundaries identified:\n" );
    for ( i = 0; i < nPartitions; i++ )
    {
        printf( "  Partition %d: %d inputs, %d outputs\n", i,
                Vec_IntSize((Vec_Int_t *)Vec_VecEntry(pIfMan->vPartInputs, i)),
                Vec_IntSize((Vec_Int_t *)Vec_VecEntry(pIfMan->vPartOutputs, i)) );
    }
    
}

/**Function*************************************************************

  Synopsis    [Checks if a node is a partition input.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
int If_ObjIsPartitionInput( If_Man_t * p, int nodeId, int partId )
{
    Vec_Int_t * vInputs;
    if ( !p->vPartInputs || partId < 0 || partId >= p->nPartitions )
        return 0;
    
    vInputs = (Vec_Int_t *)Vec_VecEntry( p->vPartInputs, partId );
    return Vec_IntFind( vInputs, nodeId ) != -1;
}

/**Function*************************************************************

  Synopsis    [Gets partition ID for a node.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
int If_ObjPartition( If_Man_t * p, If_Obj_t * pObj )
{
    if ( !p || !pObj )
        return -1;
    if ( !p->vPartition || If_ObjId(pObj) >= Vec_IntSize(p->vPartition) )
        return -1;
    return Vec_IntEntry( p->vPartition, If_ObjId(pObj) );
}

/**Function*************************************************************

  Synopsis    [Checks if cut satisfies partition constraints.]

  Description [Returns 1 if all leaves are in the same partition or are partition inputs.]
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
int If_CutCheckPartition( If_Man_t * p, If_Cut_t * pCut, int targetPartition )
{
    int i, leafId, leafPartition;
    
    if ( !p->vPartition || targetPartition < 0 )
        return 1; // No partition constraints
    
    for ( i = 0; i < (int)pCut->nLeaves; i++ )
    {
        leafId = pCut->pLeaves[i];
        
        if ( leafId >= Vec_IntSize(p->vPartition) )
            continue;
            
        leafPartition = Vec_IntEntry( p->vPartition, leafId );
        
        // CI nodes (partition = -1) can be used by any partition
        if ( leafPartition == -1 )
            continue;
            
        // Leaf must be in target partition or be a partition input
        if ( leafPartition != targetPartition )
        {
            if ( !If_ObjIsPartitionInput( p, leafId, targetPartition ) )
                return 0; // Violates partition constraint
        }
    }
    
    return 1; // Cut satisfies partition constraints
}

/**Function*************************************************************

  Synopsis    [Limits cuts to trivial cut for cross-partition fanins.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void If_ObjLimitCutsToTrivial( If_Man_t * p, If_Obj_t * pObj )
{
    If_Cut_t * pCut;
    
    if ( !pObj->pCutSet )
        return;
    
    // Ensure EstRefs is properly initialized for cross-partition fanins
    if ( pObj->EstRefs <= p->fEpsilon )
        pObj->EstRefs = (float)(pObj->nRefs > 0 ? pObj->nRefs : 1);
    
    // Keep only the trivial cut
    pCut = pObj->pCutSet->ppCuts[0];
    pObj->pCutSet->nCuts = 1;
    
    // Ensure it's a trivial cut
    pCut->nLeaves = 1;
    pCut->pLeaves[0] = If_ObjId(pObj);
    pCut->uSign = If_ObjCutSign(If_ObjId(pObj));
    pCut->Delay = If_ObjCutBest(pObj)->Delay;
    pCut->Area = If_CutLutArea(p, pCut);
}

/**Function*************************************************************

  Synopsis    [Partition-aware cut generation for AND nodes.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void If_ObjPerformMappingAndPartitionAware( If_Man_t * p, If_Obj_t * pObj, int Mode, int fPreprocess, int fFirst )
{
    If_Cut_t * pCut0, * pCut1, * pCut;
    int i, k;
    int objPartition = -1;
    int fanin0Partition = -1;
    int fanin1Partition = -1;
    int fanin0CrossPartition = 0;
    int fanin1CrossPartition = 0;
    
    // Debug info
    assert( pObj );
    assert( p );
    assert( If_ObjIsAnd(pObj) );
    assert( pObj->pFanin0 );
    assert( pObj->pFanin1 );
    
    
    // Initialize EstRefs for the current object
    if ( Mode == 0 )
        pObj->EstRefs = (float)pObj->nRefs;
    else if ( Mode == 1 )
        pObj->EstRefs = (float)((2.0 * pObj->EstRefs + pObj->nRefs) / 3.0);
    
    // Deref the selected cut
    if ( Mode && pObj->nRefs > 0 )
        If_CutAreaDeref( p, If_ObjCutBest(pObj) );
    
    // Check if fanins have cut sets (only needed if not first pass)
    if ( !fFirst && (!pObj->pFanin0->pCutSet || !pObj->pFanin1->pCutSet) )
    {
        printf( "Warning: Fanin cut sets not initialized for obj %d\n", If_ObjId(pObj) );
        printf( "  Fanin0 (ID=%d) cutset=%p\n", If_ObjId(pObj->pFanin0), pObj->pFanin0->pCutSet );
        printf( "  Fanin1 (ID=%d) cutset=%p\n", If_ObjId(pObj->pFanin1), pObj->pFanin1->pCutSet );
        // Fall back to standard mapping
        If_ObjPerformMappingAnd( p, pObj, Mode, fPreprocess, fFirst );
        return;
    }
    
    
    // Get partition information
    if ( p->vPartition )
    {
        objPartition = If_ObjPartition( p, pObj );
        fanin0Partition = If_ObjPartition( p, pObj->pFanin0 );
        fanin1Partition = If_ObjPartition( p, pObj->pFanin1 );
        
        // Check for cross-partition fanins
        fanin0CrossPartition = (fanin0Partition != objPartition && fanin0Partition >= 0 && objPartition >= 0);
        fanin1CrossPartition = (fanin1Partition != objPartition && fanin1Partition >= 0 && objPartition >= 0);
        
        // Ensure EstRefs for fanins
        if ( fanin0CrossPartition && pObj->pFanin0->EstRefs <= p->fEpsilon )
            pObj->pFanin0->EstRefs = (float)(pObj->pFanin0->nRefs > 0 ? pObj->pFanin0->nRefs : 1);
        if ( fanin1CrossPartition && pObj->pFanin1->EstRefs <= p->fEpsilon )
            pObj->pFanin1->EstRefs = (float)(pObj->pFanin1->nRefs > 0 ? pObj->pFanin1->nRefs : 1);
    }
    
    // Prepare the cutset
    If_Set_t * pCutSet = If_ManSetupNodeCutSet( p, pObj );
    
    // Get the current best cut
    pCut = If_ObjCutBest(pObj);
    if ( !fFirst )
    {
        // Recompute cut parameters
        pCut->Delay = If_CutDelay( p, pObj, pCut );
        // In partition-aware mapping, always use area flow to avoid ref counting issues
        pCut->Area = If_CutAreaFlow( p, pCut );
        
        // Save best cut
        if ( !fPreprocess || pCut->nLeaves <= 1 )
            If_CutCopy( p, pCutSet->ppCuts[pCutSet->nCuts++], pCut );
    }
    
    // Generate cuts with partition constraints
    If_ObjForEachCut( pObj->pFanin0, pCut0, i )
    {
        If_ObjForEachCut( pObj->pFanin1, pCut1, k )
        {
                
            // Get next free cut
            assert( pCutSet->nCuts <= pCutSet->nCutsMax );
            pCut = pCutSet->ppCuts[pCutSet->nCuts];
            
            // Quick feasibility check
            if ( If_WordCountOnes(pCut0->uSign | pCut1->uSign) > p->pPars->nLutSize )
                continue;
            
            // Merge cuts
            if ( !If_CutMergeOrdered( p, pCut0, pCut1, pCut ) )
                continue;
            
            // Check partition constraints - strict enforcement
            if ( p->vPartition && objPartition >= 0 )
            {
                if ( !If_CutCheckPartition( p, pCut, objPartition ) )
                    continue;
            }
            
            p->nCutsMerged++;
            p->nCutsTotal++;
            
            // Filter redundant cuts
            if ( !p->pPars->fSkipCutFilter && If_CutFilter( pCutSet, pCut, 0 ) )
                continue;
            
            // Compute cut properties
            if ( p->pPars->fTruth )
            {
                if ( !If_CutComputeTruth( p, pCut, pCut0, pCut1, pObj->fCompl0, pObj->fCompl1 ) )
                    continue;
            }
            
            // Compute delay and area
            pCut->Delay = If_CutDelay( p, pObj, pCut );
            if ( pCut->Delay == -1 )
                continue;
            
            // In partition-aware mapping, always use area flow to avoid ref counting issues
        pCut->Area = If_CutAreaFlow( p, pCut );
            
            // Insert cut into storage
            If_CutSort( p, pCutSet, pCut );
        }
    }
    
    // Make sure we have at least one cut
    if ( pCutSet->nCuts == 0 )
    {
        // Create a trivial cut
        If_ManSetupCutTriv( p, pCutSet->ppCuts[0], pObj->Id );
        pCutSet->nCuts = 1;
    }
    
    assert( pCutSet->nCuts > 0 );
    
    // Update best cut
    if ( !fPreprocess || pCutSet->ppCuts[0]->Delay <= pObj->Required + p->fEpsilon )
    {
        If_CutCopy( p, If_ObjCutBest(pObj), pCutSet->ppCuts[0] );
    }
    
    // Add trivial cut
    if ( !pObj->fSkipCut && If_ObjCutBest(pObj)->nLeaves > 1 )
    {
        If_ManSetupCutTriv( p, pCutSet->ppCuts[pCutSet->nCuts++], pObj->Id );
        assert( pCutSet->nCuts <= pCutSet->nCutsMax + 1 );
    }
    
    // Reference selected cut
    if ( Mode && pObj->nRefs > 0 )
        If_CutAreaRef( p, If_ObjCutBest(pObj) );
    
    // Free cuts
    If_ManDerefNodeCutSet( p, pObj );
}

/**Function*************************************************************

  Synopsis    [Cleanup partition information.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void If_ManCleanPartitionInfo( If_Man_t * p )
{
    if ( p->vPartition )
    {
        Vec_IntFree( p->vPartition );
        p->vPartition = NULL;
    }
    if ( p->vPartInputs )
    {
        Vec_VecFree( p->vPartInputs );
        p->vPartInputs = NULL;
    }
    if ( p->vPartOutputs )
    {
        Vec_VecFree( p->vPartOutputs );
        p->vPartOutputs = NULL;
    }
    p->nPartitions = 0;
}

////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////

ABC_NAMESPACE_IMPL_END