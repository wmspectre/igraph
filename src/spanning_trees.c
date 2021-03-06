/* -*- mode: C -*-  */
/* vim:set ts=2 sts=2 sw=2 et: */
/* 
   IGraph library.
   Copyright (C) 2011  Gabor Csardi <csardi.gabor@gmail.com>
   Rue de l'Industrie 5, Lausanne 1005, Switzerland
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 
   02110-1301 USA

*/

#include "igraph_structural.h"
#include "igraph_dqueue.h"
#include "igraph_interface.h"
#include "igraph_interrupt_internal.h"
#include "igraph_memory.h"
#include "igraph_progress.h"
#include "igraph_types_internal.h"

int igraph_i_minimum_spanning_tree_unweighted(const igraph_t *graph,
    igraph_vector_t *result);
int igraph_i_minimum_spanning_tree_prim(const igraph_t *graph,
    igraph_vector_t *result, const igraph_vector_t *weights);

/**
 * \ingroup structural
 * \function igraph_minimum_spanning_tree
 * \brief Calculates one minimum spanning tree of a graph.
 * 
 * </para><para>
 * If the graph has more minimum spanning trees (this is always the
 * case, except if it is a forest) this implementation returns only
 * the same one.
 * 
 * </para><para>
 * Directed graphs are considered as undirected for this computation.
 *
 * </para><para>
 * If the graph is not connected then its minimum spanning forest is
 * returned. This is the set of the minimum spanning trees of each
 * component.
 *
 * \param graph The graph object.
 * \param res An initialized vector, the IDs of the edges that constitute
 *        a spanning tree will be returned here. Use
 *        \ref igraph_subgraph_edges() to extract the spanning tree as
 *        a separate graph object.
 * \param weights A vector containing the weights of the edges
 *        in the same order as the simple edge iterator visits them
 *        (i.e. in increasing order of edge IDs).
 * \return Error code:
 *         \c IGRAPH_ENOMEM, not enough memory for
 *         temporary data. 
 *
 * Time complexity: O(|V|+|E|) for the unweighted case, O(|E| log |V|)
 * for the weighted case. |V| is the number of vertices, |E| the
 * number of edges in the graph. 
 *
 * \sa \ref igraph_minimum_spanning_tree_unweighted() and
 *     \ref igraph_minimum_spanning_tree_prim() if you only need the
 *     tree as a separate graph object.
 * 
 * \example examples/simple/igraph_minimum_spanning_tree.c
 */
int igraph_minimum_spanning_tree(const igraph_t* graph,
    igraph_vector_t* res, const igraph_vector_t* weights) {
  if (weights == 0)
    IGRAPH_CHECK(igraph_i_minimum_spanning_tree_unweighted(graph, res));
  else
    IGRAPH_CHECK(igraph_i_minimum_spanning_tree_prim(graph, res, weights));
  return IGRAPH_SUCCESS;
}

/**
 * \ingroup structural
 * \function igraph_minimum_spanning_tree_unweighted
 * \brief Calculates one minimum spanning tree of an unweighted graph.
 * 
 * </para><para>
 * If the graph has more minimum spanning trees (this is always the
 * case, except if it is a forest) this implementation returns only
 * the same one.
 * 
 * </para><para>
 * Directed graphs are considered as undirected for this computation.
 *
 * </para><para>
 * If the graph is not connected then its minimum spanning forest is
 * returned. This is the set of the minimum spanning trees of each
 * component.
 * \param graph The graph object.
 * \param mst The minimum spanning tree, another graph object. Do
 *        \em not initialize this object before passing it to
 *        this function, but be sure to call \ref igraph_destroy() on it if
 *        you don't need it any more.
 * \return Error code:
 *         \c IGRAPH_ENOMEM, not enough memory for
 *         temporary data. 
 *
 * Time complexity: O(|V|+|E|),
 * |V| is the 
 * number of vertices, |E| the number
 * of edges in the graph. 
 *
 * \sa \ref igraph_minimum_spanning_tree_prim() for weighted graphs,
 *     \ref igraph_minimum_spanning_tree() if you need the IDs of the
 *     edges that constitute the spanning tree.
 */

int igraph_minimum_spanning_tree_unweighted(const igraph_t *graph, 
					    igraph_t *mst) {
  igraph_vector_t edges=IGRAPH_VECTOR_NULL;

  IGRAPH_VECTOR_INIT_FINALLY(&edges, igraph_vcount(graph)-1);
  IGRAPH_CHECK(igraph_i_minimum_spanning_tree_unweighted(graph, &edges));
  IGRAPH_CHECK(igraph_subgraph_edges(graph, mst,
        igraph_ess_vector(&edges), /* delete_vertices = */ 0));
  igraph_vector_destroy(&edges);
  IGRAPH_FINALLY_CLEAN(1);
  
  return 0;
}

/**
 * \ingroup structural
 * \function igraph_minimum_spanning_tree_prim
 * \brief Calculates one minimum spanning tree of a weighted graph.
 *
 * </para><para>
 * This function uses Prim's method for carrying out the computation,
 * see Prim, R.C.: Shortest connection networks and some
 * generalizations, Bell System Technical
 * Journal, Vol. 36, 
 * 1957, 1389--1401.
 * 
 * </para><para>
 * If the graph has more than one minimum spanning tree, the current
 * implementation returns always the same one.
 *
 * </para><para>
 * Directed graphs are considered as undirected for this computation. 
 * 
 * </para><para>
 * If the graph is not connected then its minimum spanning forest is
 * returned. This is the set of the minimum spanning trees of each
 * component.
 * 
 * \param graph The graph object.
 * \param mst The result of the computation, a graph object containing
 *        the minimum spanning tree of the graph.
 *        Do \em not initialize this object before passing it to
 *        this function, but be sure to call \ref igraph_destroy() on it if
 *        you don't need it any more.
 * \param weights A vector containing the weights of the edges
 *        in the same order as the simple edge iterator visits them
 *        (i.e. in increasing order of edge IDs).
 * \return Error code:
 *         \c IGRAPH_ENOMEM, not enough memory.
 *         \c IGRAPH_EINVAL, length of weight vector does not
 *           match number of edges.
 *
 * Time complexity: O(|E| log |V|),
 * |V| is the number of vertices,
 * |E| the number of edges in the 
 * graph. 
 *
 * \sa \ref igraph_minimum_spanning_tree_unweighted() for unweighted graphs,
 *     \ref igraph_minimum_spanning_tree() if you need the IDs of the
 *     edges that constitute the spanning tree.
 * 
 * \example examples/simple/igraph_minimum_spanning_tree.c
 */

int igraph_minimum_spanning_tree_prim(const igraph_t *graph, igraph_t *mst,
				      const igraph_vector_t *weights) {
  igraph_vector_t edges=IGRAPH_VECTOR_NULL;

  IGRAPH_VECTOR_INIT_FINALLY(&edges, igraph_vcount(graph)-1);
  IGRAPH_CHECK(igraph_i_minimum_spanning_tree_prim(graph, &edges, weights));
  IGRAPH_CHECK(igraph_subgraph_edges(graph, mst,
        igraph_ess_vector(&edges), /* delete_vertices = */ 0));
  igraph_vector_destroy(&edges);
  IGRAPH_FINALLY_CLEAN(1);
  
  return 0;
}


int igraph_i_minimum_spanning_tree_unweighted(const igraph_t* graph,
    igraph_vector_t* res) {

  long int no_of_nodes=igraph_vcount(graph);
  long int no_of_edges=igraph_ecount(graph);
  char *already_added;
  char *added_edges;
  
  igraph_dqueue_t q=IGRAPH_DQUEUE_NULL;
  igraph_vector_t tmp=IGRAPH_VECTOR_NULL;
  long int i, j;

  igraph_vector_clear(res);

  added_edges=igraph_Calloc(no_of_edges, char);
  if (added_edges==0) {
    IGRAPH_ERROR("unweighted spanning tree failed", IGRAPH_ENOMEM);
  }
  IGRAPH_FINALLY(igraph_free, added_edges);
  already_added=igraph_Calloc(no_of_nodes, char);
  if (already_added==0) {
    IGRAPH_ERROR("unweighted spanning tree failed", IGRAPH_ENOMEM);
  }
  IGRAPH_FINALLY(igraph_free, already_added);
  IGRAPH_VECTOR_INIT_FINALLY(&tmp, 0);
  IGRAPH_DQUEUE_INIT_FINALLY(&q, 100);
  
  for (i=0; i<no_of_nodes; i++) {
    if (already_added[i]>0) { continue; }

    IGRAPH_ALLOW_INTERRUPTION();

    already_added[i]=1;
    IGRAPH_CHECK(igraph_dqueue_push(&q, i));
    while (! igraph_dqueue_empty(&q)) {
      long int act_node=(long int) igraph_dqueue_pop(&q);
      IGRAPH_CHECK(igraph_incident(graph, &tmp, (igraph_integer_t) act_node,
				   IGRAPH_ALL));
      for (j=0; j<igraph_vector_size(&tmp); j++) {
        long int edge=(long int) VECTOR(tmp)[j];
        if (added_edges[edge]==0) {
          igraph_integer_t from, to;
          igraph_edge(graph, (igraph_integer_t) edge, &from, &to);
          if (act_node==to) { to=from; }
          if (already_added[(long int) to]==0) {
            already_added[(long int) to]=1;
            added_edges[edge]=1;
            IGRAPH_CHECK(igraph_vector_push_back(res, edge));
            IGRAPH_CHECK(igraph_dqueue_push(&q, to));
          }
        }
      }
    }
  }
  
  igraph_dqueue_destroy(&q);
  igraph_Free(already_added);
  igraph_vector_destroy(&tmp);
  igraph_Free(added_edges);
  IGRAPH_FINALLY_CLEAN(4);

  return IGRAPH_SUCCESS;
}

int igraph_i_minimum_spanning_tree_prim(const igraph_t* graph,
    igraph_vector_t* res, const igraph_vector_t *weights) {

  long int no_of_nodes=igraph_vcount(graph);
  long int no_of_edges=igraph_ecount(graph);
  char *already_added;
  char *added_edges;

  igraph_d_indheap_t heap=IGRAPH_D_INDHEAP_NULL;
  igraph_integer_t mode=IGRAPH_ALL;
  
  igraph_vector_t adj;

  long int i, j;

  igraph_vector_clear(res);

  if (weights == 0)
    return igraph_i_minimum_spanning_tree_unweighted(graph, res);

  if (igraph_vector_size(weights) != igraph_ecount(graph)) {
    IGRAPH_ERROR("Invalid weights length", IGRAPH_EINVAL);
  }

  added_edges=igraph_Calloc(no_of_edges, char);
  if (added_edges==0) {
    IGRAPH_ERROR("prim spanning tree failed", IGRAPH_ENOMEM);
  }
  IGRAPH_FINALLY(igraph_free, added_edges);
  already_added=igraph_Calloc(no_of_nodes, char);
  if (already_added == 0) {
    IGRAPH_ERROR("prim spanning tree failed", IGRAPH_ENOMEM);
  }
  IGRAPH_FINALLY(igraph_free, already_added);
  IGRAPH_CHECK(igraph_d_indheap_init(&heap, 0));
  IGRAPH_FINALLY(igraph_d_indheap_destroy, &heap);
  IGRAPH_VECTOR_INIT_FINALLY(&adj, 0);

  for (i=0; i<no_of_nodes; i++) {
    if (already_added[i]>0) { continue; }
    IGRAPH_ALLOW_INTERRUPTION();

    already_added[i]=1;
    /* add all edges of the first vertex */
    igraph_incident(graph, &adj, (igraph_integer_t) i, (igraph_neimode_t) mode);
    for (j=0; j<igraph_vector_size(&adj); j++) {
      long int edgeno=(long int) VECTOR(adj)[j];
      igraph_integer_t edgefrom, edgeto;
      long int neighbor;
      igraph_edge(graph, (igraph_integer_t) edgeno, &edgefrom, &edgeto);
      neighbor= edgefrom != i ? edgefrom : edgeto;
      if (already_added[neighbor] == 0) {
	IGRAPH_CHECK(igraph_d_indheap_push(&heap, -VECTOR(*weights)[edgeno], i,
					   edgeno));
      }
    }

    while(! igraph_d_indheap_empty(&heap)) {
      /* Get minimal edge */
      long int from, edge;
      igraph_integer_t tmp, to;
      igraph_d_indheap_max_index(&heap, &from, &edge);
      igraph_edge(graph, (igraph_integer_t) edge, &tmp, &to);
      
      /* Erase it */
      igraph_d_indheap_delete_max(&heap);

      /* Is this edge already included? */
      if (added_edges[edge]==0) {
        if (from==to) { to=tmp; }
        /* Does it point to a visited node? */      
        if (already_added[(long int)to]==0) {
          already_added[(long int)to]=1;
          added_edges[edge]=1;
          IGRAPH_CHECK(igraph_vector_push_back(res, edge));
          /* add all outgoing edges */
          igraph_incident(graph, &adj, to, (igraph_neimode_t) mode);
          for (j=0; j<igraph_vector_size(&adj); j++) {
            long int edgeno=(long int) VECTOR(adj)[j];
            igraph_integer_t edgefrom, edgeto;
            long int neighbor;
            igraph_edge(graph, (igraph_integer_t) edgeno, &edgefrom, &edgeto);
            neighbor= edgefrom != to ? edgefrom : edgeto;
            if (already_added[neighbor] == 0) {
              IGRAPH_CHECK(igraph_d_indheap_push(&heap, -VECTOR(*weights)[edgeno], to,
                   edgeno));
            }
          }
        } /* for */
      } /* if !already_added */
    } /* while in the same component */
  } /* for all nodes */

  igraph_d_indheap_destroy(&heap);
  igraph_Free(already_added);
  igraph_vector_destroy(&adj);
  igraph_Free(added_edges);
  IGRAPH_FINALLY_CLEAN(4);

  return IGRAPH_SUCCESS;
}


