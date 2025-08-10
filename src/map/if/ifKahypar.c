/**CFile****************************************************************

  FileName    [ifKahypar.c]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [FPGA mapping based on priority cuts with hypergraph partitioning.]

  Synopsis    [KaHyPar hypergraph partitioning implementation.]

  Author      [Based on LSOracle implementation]
  
  Affiliation [ABC team]

  Date        [Ver. 1.0. Started - January 2025.]

***********************************************************************/

#ifdef ABC_USE_KAHYPAR

#include "ifKahypar.h"
#include "base/abc/abc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Include KaHyPar library header
#include "libkahypar.h"

ABC_NAMESPACE_IMPL_START

////////////////////////////////////////////////////////////////////////
///                        DECLARATIONS                              ///
////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////
///                     FUNCTION DEFINITIONS                         ///
////////////////////////////////////////////////////////////////////////

/**Function*************************************************************

  Synopsis    [Allocates KaHyPar parameters.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
Kahypar_Par_t * Kahypar_ParAlloc()
{
    Kahypar_Par_t * p = ABC_CALLOC( Kahypar_Par_t, 1 );
    Kahypar_ParSetDefault( p );
    return p;
}

/**Function*************************************************************

  Synopsis    [Frees KaHyPar parameters.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Kahypar_ParFree( Kahypar_Par_t * p )
{
    if ( p->pConfigFile )
        ABC_FREE( p->pConfigFile );
    ABC_FREE( p );
}

/**Function*************************************************************

  Synopsis    [Sets default KaHyPar parameters.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Kahypar_ParSetDefault( Kahypar_Par_t * p )
{
    memset( p, 0, sizeof(Kahypar_Par_t) );
    p->nPartitions     = 2;
    p->dImbalance      = 0.9;
    p->pConfigFile     = NULL;
    p->fVerbose        = 0;
    p->fUseNodeWeights = 0;
    p->fUseEdgeWeights = 0;
}

/**Function*************************************************************

  Synopsis    [Allocates KaHyPar result structure.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
Kahypar_Result_t * Kahypar_ResultAlloc( int nVertices )
{
    Kahypar_Result_t * p = ABC_CALLOC( Kahypar_Result_t, 1 );
    p->nVertices = nVertices;
    p->vPartition = Vec_IntAlloc( nVertices );
    Vec_IntFill( p->vPartition, nVertices, -1 );
    p->fSuccess = 0;
    return p;
}

/**Function*************************************************************

  Synopsis    [Frees KaHyPar result structure.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Kahypar_ResultFree( Kahypar_Result_t * p )
{
    if ( p->vPartition )
        Vec_IntFree( p->vPartition );
    ABC_FREE( p );
}

/**Function*************************************************************

  Synopsis    [Creates temporary KaHyPar configuration file.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
int Kahypar_CreateTempConfig( char * pFileName )
{
    FILE * pFile;
    
    // Create temporary file
    strcpy( pFileName, "/tmp/abc_kahypar_XXXXXX" );
    int fd = mkstemp( pFileName );
    if ( fd == -1 )
    {
        printf( "Kahypar_CreateTempConfig(): Cannot create temporary config file.\n" );
        return 0;
    }
    
    pFile = fdopen( fd, "w" );
    if ( pFile == NULL )
    {
        close( fd );
        printf( "Kahypar_CreateTempConfig(): Cannot open temporary config file for writing.\n" );
        return 0;
    }
    
    // Write default configuration
    fprintf( pFile, "%s", KAHYPAR_DEFAULT_CONFIG_STR );
    fclose( pFile );
    
    return 1;
}

/**Function*************************************************************

  Synopsis    [Partitions hypergraph using KaHyPar.]

  Description [Based on LSOracle kahypar_partitioner implementation]
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
Kahypar_Result_t * Kahypar_PartitionHypergraph( Aig_Hyper_t * pHyper, Kahypar_Par_t * pPars )
{
    kahypar_context_t * pContext;
    kahypar_hypergraph_t * pKahyparHyper;
    Kahypar_Result_t * pResult;
    Vec_Int_t * vHyperedges, * vIndices, * vWeights;
    char pTempConfigFile[1000];
    int i, nVertices, nHyperedges, nPins;
    int * pPartition;
    kahypar_hyperedge_weight_t objective = 0;
    
    // Allocate result structure
    nVertices = pHyper->nVertices;
    pResult = Kahypar_ResultAlloc( nVertices );
    pResult->nPartitions = pPars->nPartitions;
    
    if ( pPars->fVerbose )
    {
        printf( "KaHyPar partitioning: %d vertices, %d hyperedges, %d partitions\n", 
                pHyper->nVertices, pHyper->nHyperedges, pPars->nPartitions );
    }
    
    // Handle trivial case
    if ( pPars->nPartitions == 1 )
    {
        Vec_IntFill( pResult->vPartition, nVertices, 0 );
        pResult->fSuccess = 1;
        return pResult;
    }
    
    // Export hypergraph for KaHyPar
    Vec_Int_t * vVertexWeights = NULL;
    Aig_HyperExportForPartitioning( pHyper, &vHyperedges, &vIndices, &vWeights, &vVertexWeights );
    nHyperedges = pHyper->nHyperedges;
    nPins = Vec_IntSize( vHyperedges );
    
    // Verify export consistency
    if ( Vec_IntSize(vIndices) != nHyperedges + 1 )
    {
        printf( "Error: Indices vector size mismatch. Expected %d, got %d\n", 
                nHyperedges + 1, Vec_IntSize(vIndices) );
        Vec_IntFree( vHyperedges );
        Vec_IntFree( vIndices );
        Vec_IntFree( vWeights );
        if ( vVertexWeights )
            Vec_IntFree( vVertexWeights );
        return pResult;
    }
    
    if ( pPars->fVerbose )
    {
        printf( "Hypergraph exported: %d pins, %d indices\n", nPins, Vec_IntSize(vIndices) );
    }
    
    // Create KaHyPar context
    pContext = kahypar_context_new();
    if ( pContext == NULL )
    {
        printf( "Kahypar_PartitionHypergraph(): Failed to create KaHyPar context.\n" );
        Vec_IntFree( vHyperedges );
        Vec_IntFree( vIndices );
        Vec_IntFree( vWeights );
        if ( vVertexWeights )
            Vec_IntFree( vVertexWeights );
        return pResult;
    }
    
    // Configure context from file
    if ( pPars->pConfigFile )
    {
        kahypar_configure_context_from_file( pContext, pPars->pConfigFile );
        if ( pPars->fVerbose )
            printf( "Using KaHyPar config file: %s\n", pPars->pConfigFile );
    }
    else
    {
        // Create temporary configuration file
        if ( !Kahypar_CreateTempConfig( pTempConfigFile ) )
        {
            printf( "Kahypar_PartitionHypergraph(): Failed to create temporary config file.\n" );
            kahypar_context_free( pContext );
            Vec_IntFree( vHyperedges );
            Vec_IntFree( vIndices );
            Vec_IntFree( vWeights );
            if ( vVertexWeights )
                Vec_IntFree( vVertexWeights );
            return pResult;
        }
        kahypar_configure_context_from_file( pContext, pTempConfigFile );
        if ( pPars->fVerbose )
            printf( "Using default KaHyPar configuration\n" );
    }
    
    // Prepare data for KaHyPar
    size_t * pHyperedgeIndices = ABC_ALLOC( size_t, nHyperedges + 1 );
    kahypar_hyperedge_id_t * pHyperedges = ABC_ALLOC( kahypar_hyperedge_id_t, nPins );
    kahypar_hyperedge_weight_t * pEdgeWeights = NULL;
    kahypar_hypernode_weight_t * pNodeWeights = NULL;
    
    // Copy hyperedge indices
    for ( i = 0; i <= nHyperedges; i++ )
        pHyperedgeIndices[i] = Vec_IntEntry( vIndices, i );
        
    // Copy hyperedges
    for ( i = 0; i < nPins; i++ )
        pHyperedges[i] = Vec_IntEntry( vHyperedges, i );
    
    // Set edge weights
    if ( pPars->fUseEdgeWeights )
    {
        pEdgeWeights = ABC_ALLOC( kahypar_hyperedge_weight_t, nHyperedges );
        // Use actual edge weights from hypergraph if available
        if ( pHyper->vEdgeWeights && Vec_IntSize(pHyper->vEdgeWeights) == nHyperedges )
        {
            for ( i = 0; i < nHyperedges; i++ )
                pEdgeWeights[i] = Vec_IntEntry( pHyper->vEdgeWeights, i );
        }
        else
        {
            // Default to uniform weights
            for ( i = 0; i < nHyperedges; i++ )
                pEdgeWeights[i] = 1;
        }
    }
    
    // Set node weights
    if ( pPars->fUseNodeWeights )
    {
        pNodeWeights = ABC_ALLOC( kahypar_hypernode_weight_t, nVertices );
        // Use actual vertex weights from exported data if available
        if ( vVertexWeights && Vec_IntSize(vVertexWeights) >= nVertices )
        {
            for ( i = 0; i < nVertices; i++ )
                pNodeWeights[i] = Vec_IntEntry( vVertexWeights, i );
        }
        else
        {
            // Default to uniform weights
            for ( i = 0; i < nVertices; i++ )
                pNodeWeights[i] = 1;
        }
    }
    
    // Create KaHyPar hypergraph
    pKahyparHyper = kahypar_create_hypergraph( pPars->nPartitions,
                                               nVertices,
                                               nHyperedges,
                                               pHyperedgeIndices,
                                               pHyperedges,
                                               pEdgeWeights,
                                               pNodeWeights );
    
    if ( pKahyparHyper == NULL )
    {
        printf( "Kahypar_PartitionHypergraph(): Failed to create KaHyPar hypergraph.\n" );
        goto cleanup;
    }
    
    // Allocate partition array
    pPartition = ABC_ALLOC( kahypar_partition_id_t, nVertices );
    for ( i = 0; i < nVertices; i++ )
        pPartition[i] = -1;
    
    // Perform partitioning
    kahypar_partition_hypergraph( pKahyparHyper, 
                                  pPars->nPartitions,
                                  pPars->dImbalance,
                                  &objective,
                                  pContext,
                                  pPartition );
    
    // Copy results
    for ( i = 0; i < nVertices; i++ )
        Vec_IntWriteEntry( pResult->vPartition, i, pPartition[i] );
    
    pResult->nCutEdges = (int)objective;
    pResult->fSuccess = 1;
    
    if ( pPars->fVerbose )
    {
        printf( "KaHyPar partitioning completed: objective = %d\n", (int)objective );
    }
    
    // Cleanup
    ABC_FREE( pPartition );
    kahypar_hypergraph_free( pKahyparHyper );
    
cleanup:
    // Cleanup KaHyPar data
    ABC_FREE( pHyperedgeIndices );
    ABC_FREE( pHyperedges );
    if ( pEdgeWeights ) ABC_FREE( pEdgeWeights );
    if ( pNodeWeights ) ABC_FREE( pNodeWeights );
    
    kahypar_context_free( pContext );
    
    // Remove temporary config file if created
    if ( !pPars->pConfigFile )
        unlink( pTempConfigFile );
    
    // Free exported data
    Vec_IntFree( vHyperedges );
    Vec_IntFree( vIndices );
    Vec_IntFree( vWeights );
    if ( vVertexWeights )
        Vec_IntFree( vVertexWeights );
    
    return pResult;
}

/**Function*************************************************************

  Synopsis    [Prints KaHyPar partitioning result.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Kahypar_PrintResult( Kahypar_Result_t * pResult )
{
    int i, * pPartCounts;
    
    if ( !pResult->fSuccess )
    {
        printf( "KaHyPar partitioning failed.\n" );
        return;
    }
    
    printf( "KaHyPar partitioning result:\n" );
    printf( "  Vertices: %d\n", pResult->nVertices );
    printf( "  Partitions: %d\n", pResult->nPartitions );
    printf( "  Cut hyperedges: %d\n", pResult->nCutEdges );
    
    // Count vertices per partition
    pPartCounts = ABC_CALLOC( int, pResult->nPartitions );
    for ( i = 0; i < pResult->nVertices; i++ )
    {
        int part = Vec_IntEntry( pResult->vPartition, i );
        if ( part >= 0 && part < pResult->nPartitions )
            pPartCounts[part]++;
    }
    
    printf( "  Partition sizes: " );
    for ( i = 0; i < pResult->nPartitions; i++ )
        printf( "%d ", pPartCounts[i] );
    printf( "\n" );
    
    ABC_FREE( pPartCounts );
}

/**Function*************************************************************

  Synopsis    [Tests KaHyPar partitioning on AIG network.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
int Kahypar_TestPartition( void * pNtk, int nPartitions )
{
    Aig_Hyper_t * pHyper;
    Kahypar_Par_t * pPars;
    Kahypar_Result_t * pResult;
    int fSuccess = 0;
    
    // Build hypergraph
    pHyper = Aig_NtkBuildHypergraph( pNtk );
    if ( pHyper == NULL )
    {
        printf( "Kahypar_TestPartition(): Failed to build hypergraph.\n" );
        return 0;
    }
    
    // Setup partitioning parameters
    pPars = Kahypar_ParAlloc();
    pPars->nPartitions = nPartitions;
    pPars->fVerbose = 1;
    
    // Perform partitioning
    pResult = Kahypar_PartitionHypergraph( pHyper, pPars );
    
    if ( pResult && pResult->fSuccess )
    {
        Kahypar_PrintResult( pResult );
        
        // Apply partition result to AIG network
        if ( Aig_ApplyPartitionResult( pNtk, pHyper, pResult->vPartition, pResult->nPartitions ) )
        {
            printf( "Partition result successfully applied to AIG network.\n" );
            fSuccess = 1;
        }
        else
        {
            printf( "Warning: Failed to apply partition result to AIG network.\n" );
            fSuccess = 1; // Still consider partitioning successful
        }
    }
    
    // Cleanup
    if ( pResult )
        Kahypar_ResultFree( pResult );
    Kahypar_ParFree( pPars );
    Aig_HyperFree( pHyper );
    
    return fSuccess;
}

/**Function*************************************************************

  Synopsis    [Tests timing-aware KaHyPar partitioning on AIG network.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
int Kahypar_TestTimingAwarePartition( void * pNtk, int nPartitions )
{
    Aig_Hyper_t * pHyper;
    Kahypar_Par_t * pPars;
    Kahypar_Result_t * pResult;
    int fSuccess = 0;
    
    // Declare the timing-aware function
    extern Aig_Hyper_t * Aig_NtkBuildTimingAwareHypergraph( void * pNtkVoid );
    
    // Build timing-aware hypergraph
    pHyper = Aig_NtkBuildTimingAwareHypergraph( pNtk );
    if ( pHyper == NULL )
    {
        printf( "Kahypar_TestTimingAwarePartition(): Failed to build timing-aware hypergraph.\n" );
        return 0;
    }
    
    // Setup partitioning parameters
    pPars = Kahypar_ParAlloc();
    pPars->nPartitions = nPartitions;
    pPars->fVerbose = 1;
    pPars->fUseNodeWeights = 1;  // Use node weights for timing
    pPars->fUseEdgeWeights = 1;  // Use edge weights for timing
    
    // Perform partitioning
    pResult = Kahypar_PartitionHypergraph( pHyper, pPars );
    
    if ( pResult && pResult->fSuccess )
    {
        Kahypar_PrintResult( pResult );
        
        // Apply partition result to AIG network
        if ( Aig_ApplyPartitionResult( pNtk, pHyper, pResult->vPartition, pResult->nPartitions ) )
        {
            printf( "Timing-aware partition result successfully applied to AIG network.\n" );
            fSuccess = 1;
        }
        else
        {
            printf( "Warning: Failed to apply timing-aware partition result to AIG network.\n" );
            fSuccess = 1; // Still consider partitioning successful
        }
    }
    
    // Cleanup
    if ( pResult )
        Kahypar_ResultFree( pResult );
    Kahypar_ParFree( pPars );
    Aig_HyperFree( pHyper );
    
    return fSuccess;
}

/**Function*************************************************************

  Synopsis    [Gets partition result from KaHyPar.]

  Description [Returns partition assignment vector for IF mapping.]
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
int Kahypar_GetPartition( void * pNtk, int nPartitions, Vec_Int_t ** pvPartition )
{
    Aig_Hyper_t * pHyper;
    Kahypar_Par_t * pPars;
    Kahypar_Result_t * pResult;
    int fSuccess = 0;
    
    *pvPartition = NULL;
    
    // Build hypergraph
    pHyper = Aig_NtkBuildHypergraph( pNtk );
    if ( pHyper == NULL )
    {
        printf( "Kahypar_GetPartition(): Failed to build hypergraph.\n" );
        return 0;
    }
    
    // Setup partitioning parameters
    pPars = Kahypar_ParAlloc();
    pPars->nPartitions = nPartitions;
    pPars->fVerbose = 1;
    
    // Perform partitioning
    pResult = Kahypar_PartitionHypergraph( pHyper, pPars );
    
    if ( pResult && pResult->fSuccess )
    {
        Kahypar_PrintResult( pResult );
        
        // Apply partition result to AIG network
        if ( Aig_ApplyPartitionResult( pNtk, pHyper, pResult->vPartition, pResult->nPartitions ) )
        {
            printf( "Partition result successfully applied to AIG network.\n" );
        }
        
        // Return the partition vector (transfer ownership)
        *pvPartition = pResult->vPartition;
        pResult->vPartition = NULL; // Prevent double free
        fSuccess = 1;
    }
    
    // Cleanup
    if ( pResult )
        Kahypar_ResultFree( pResult );
    Kahypar_ParFree( pPars );
    Aig_HyperFree( pHyper );
    
    return fSuccess;
}

/**Function*************************************************************

  Synopsis    [Gets timing-aware partition result from KaHyPar.]

  Description [Returns partition assignment vector for IF mapping with timing awareness.]
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
int Kahypar_GetTimingAwarePartition( void * pNtk, int nPartitions, Vec_Int_t ** pvPartition )
{
    Aig_Hyper_t * pHyper;
    Kahypar_Par_t * pPars;
    Kahypar_Result_t * pResult;
    int fSuccess = 0;
    
    // Declare the timing-aware function
    extern Aig_Hyper_t * Aig_NtkBuildTimingAwareHypergraph( void * pNtkVoid );
    
    *pvPartition = NULL;
    
    // Build timing-aware hypergraph
    pHyper = Aig_NtkBuildTimingAwareHypergraph( pNtk );
    if ( pHyper == NULL )
    {
        printf( "Kahypar_GetTimingAwarePartition(): Failed to build timing-aware hypergraph.\n" );
        return 0;
    }
    
    // Setup partitioning parameters
    pPars = Kahypar_ParAlloc();
    pPars->nPartitions = nPartitions;
    pPars->fVerbose = 1;
    pPars->fUseNodeWeights = 1;  // Use node weights for timing
    pPars->fUseEdgeWeights = 1;  // Use edge weights for timing
    
    // Perform partitioning
    pResult = Kahypar_PartitionHypergraph( pHyper, pPars );
    
    if ( pResult && pResult->fSuccess )
    {
        Kahypar_PrintResult( pResult );
        
        // Apply partition result to AIG network
        if ( Aig_ApplyPartitionResult( pNtk, pHyper, pResult->vPartition, pResult->nPartitions ) )
        {
            printf( "Timing-aware partition result successfully applied to AIG network.\n" );
        }
        
        // Return the partition vector (transfer ownership)
        *pvPartition = pResult->vPartition;
        pResult->vPartition = NULL; // Prevent double free
        fSuccess = 1;
    }
    
    // Cleanup
    if ( pResult )
        Kahypar_ResultFree( pResult );
    Kahypar_ParFree( pPars );
    Aig_HyperFree( pHyper );
    
    return fSuccess;
}

ABC_NAMESPACE_IMPL_END

#endif // ABC_USE_KAHYPAR