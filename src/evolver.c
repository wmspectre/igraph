/* -*- mode: C -*-  */
/* 
   IGraph R package.
   Copyright (C) 2007  Gabor Csardi <csardi@rmki.kfki.hu>
   MTA RMKI, Konkoly-Thege Miklos st. 29-33, Budapest 1121, Hungary
   
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

#include "igraph.h"
#include "memory.h"

#include <math.h>

/***********************************************/
/* in-degree                                   */
/***********************************************/

int igraph_evolver_d(const igraph_t *graph,
		     igraph_integer_t niter,
		     igraph_vector_t *kernel,
		     igraph_vector_t *sd,
		     igraph_vector_t *norm,
		     igraph_vector_t *cites,
		     igraph_vector_t *expected,
		     igraph_real_t *logprob,
		     igraph_real_t *lognull,
		     const igraph_vector_t *debug,
		     igraph_vector_ptr_t *debugres) {

  long int no_of_nodes=igraph_vcount(graph);
  igraph_vector_t st;
  long int i;
  igraph_integer_t maxdegree;
  
  IGRAPH_VECTOR_INIT_FINALLY(&st, no_of_nodes);
  for (i=0; i<no_of_nodes; i++) {
    VECTOR(st)[i]=1;
  }
  
  IGRAPH_CHECK(igraph_maxdegree(graph, &maxdegree, igraph_vss_all(),
				IGRAPH_IN, IGRAPH_LOOPS));  
  
  for (i=0; i<niter; i++) {

    IGRAPH_ALLOW_INTERRUPTION();
    
    if (i+1!=niter) {		/* not the last iteration */
      /* measure */
      IGRAPH_CHECK(igraph_evolver_mes_d(graph, kernel, 0 /*sd*/, 0 /*norm*/, 
					0 /*cites*/, 0 /*debug*/, 0 /*debugres*/,
					&st, maxdegree));
      
      /* normalize */
      igraph_vector_multiply(kernel, 1/igraph_vector_sum(kernel));
      
      /* update st */
      IGRAPH_CHECK(igraph_evolver_st_d(graph, &st, kernel));
    } else {			/* last iteration */
      /* measure */
      IGRAPH_CHECK(igraph_evolver_mes_d(graph, kernel, sd, norm, cites, debug,
					debugres, &st, maxdegree));
      
      /* normalize */
      igraph_vector_multiply(kernel, 1/igraph_vector_sum(kernel));
      
      /* update st */
      IGRAPH_CHECK(igraph_evolver_st_d(graph, &st, kernel));
      
      /* expected number of citations */
      if (expected) {
	IGRAPH_CHECK(igraph_evolver_exp_d(graph, expected, kernel,
					  &st, maxdegree));
      }
      
      /* error calculation */
      if (logprob || lognull) {
	IGRAPH_CHECK(igraph_evolver_error_d(graph, kernel, &st, maxdegree,
					    logprob, lognull));
      }
    }
    
  }
  
  igraph_vector_destroy(&st);
  IGRAPH_FINALLY_CLEAN(1);

  return 0;
}

int igraph_evolver_mes_d(const igraph_t *graph,
			 igraph_vector_t *kernel,
			 igraph_vector_t *sd,
			 igraph_vector_t *norm,
			 igraph_vector_t *cites,
			 const igraph_vector_t *debug,
			 igraph_vector_ptr_t *debugres,
			 const igraph_vector_t *st,
			 igraph_integer_t maxind) {
  
  long int classes=maxind+1;
  long int no_of_nodes=igraph_vcount(graph);
  
  igraph_vector_t indegree;
  igraph_vector_t v_normfact, *normfact;
  igraph_vector_t ntk, ch;
  igraph_vector_t v_notnull, *notnull;

  igraph_vector_t neis;
  
  long int node;
  long int i;
  long int edges=0;
  
  IGRAPH_VECTOR_INIT_FINALLY(&indegree, no_of_nodes);
  IGRAPH_VECTOR_INIT_FINALLY(&ntk, classes);
  IGRAPH_VECTOR_INIT_FINALLY(&ch, classes);
  IGRAPH_VECTOR_INIT_FINALLY(&neis, 0);
  if (norm) {
    normfact=norm;
    IGRAPH_CHECK(igraph_vector_resize(normfact, classes));
    igraph_vector_null(normfact);
  } else {
    normfact=&v_normfact;
    IGRAPH_VECTOR_INIT_FINALLY(normfact, classes);
  }
  if (cites) {
    notnull=cites;
    IGRAPH_CHECK(igraph_vector_resize(notnull, classes));
    igraph_vector_null(notnull);
  } else {
    notnull=&v_notnull;
    IGRAPH_VECTOR_INIT_FINALLY(notnull, classes);
  }
  
  IGRAPH_CHECK(igraph_vector_resize(kernel, classes));
  igraph_vector_null(kernel);
  if (sd) { 
    IGRAPH_CHECK(igraph_vector_resize(sd, classes)); 
    igraph_vector_null(sd);
  }
  
  for (node=0; node<no_of_nodes; node++) {
    
    IGRAPH_ALLOW_INTERRUPTION();
    
    /* Estimate A() */
    IGRAPH_CHECK(igraph_neighbors(graph, &neis, node, IGRAPH_OUT));
    for (i=0; i<igraph_vector_size(&neis); i++) {
      long int to=VECTOR(neis)[i];
      long int xidx=VECTOR(indegree)[to];
      
      double xk=VECTOR(*st)[node-1]/VECTOR(ntk)[xidx];
      double oldm=VECTOR(*kernel)[xidx];
      VECTOR(*notnull)[xidx]+=1;
      VECTOR(*kernel)[xidx] += (xk-oldm)/VECTOR(*notnull)[xidx];
      if (sd) {
	VECTOR(*sd)[xidx] += (xk-oldm)*(xk-VECTOR(*kernel)[xidx]);
      }
      /* TODO: debug */
    }
    
    /* Update ntk & co */
    edges += igraph_vector_size(&neis);
    for (i=0; i<igraph_vector_size(&neis); i++) {
      long int to=VECTOR(neis)[i];
      long int xidx=VECTOR(indegree)[to];
      
      VECTOR(indegree)[to] += 1;
      VECTOR(ntk)[xidx] -= 1;
      if (VECTOR(ntk)[xidx]==0) {
	VECTOR(*normfact)[xidx] += (edges-VECTOR(ch)[xidx]);
      }
      VECTOR(ntk)[xidx+1] += 1;
      if (VECTOR(ntk)[xidx+1]==1) {
	VECTOR(ch)[xidx+1]=edges;
      }
    }
    VECTOR(ntk)[0] += 1;
    if (VECTOR(ntk)[0]==1) {
      VECTOR(ch)[0]=edges;
    }
  }
  
  /* Make normfact up to date, calculate mean, sd */
  for (i=0; i<classes; i++) {
    igraph_real_t oldmean;
    if (VECTOR(ntk)[i] != 0) {
      VECTOR(*normfact)[i] += (edges-VECTOR(ch)[i]);
    }
    if (VECTOR(*normfact)[i]==0) {
      VECTOR(*kernel)[i]=0;
      VECTOR(*normfact)[i]=1;
    }
    oldmean=VECTOR(*kernel)[i];
    VECTOR(*kernel)[i] *= VECTOR(*notnull)[i] / VECTOR(*normfact)[i];
    if (sd) {
      VECTOR(*sd)[i] += oldmean * oldmean * VECTOR(*notnull)[i] *
	(1-VECTOR(*notnull)[i]/VECTOR(*normfact)[i]);
      VECTOR(*sd)[i] = sqrt(VECTOR(*sd)[i]/(VECTOR(*normfact)[i]-1));
    }
  }
  
  if (!cites) {
    igraph_vector_destroy(notnull);
    IGRAPH_FINALLY_CLEAN(1);
  }
  if (!norm) {
    igraph_vector_destroy(normfact);
    IGRAPH_FINALLY_CLEAN(1);
  }
  igraph_vector_destroy(&neis);
  igraph_vector_destroy(&ch);
  igraph_vector_destroy(&ntk);
  igraph_vector_destroy(&indegree);
  IGRAPH_FINALLY_CLEAN(4);
	
  return 0;
}

int igraph_evolver_st_d(const igraph_t *graph,
			igraph_vector_t *st,
			const igraph_vector_t *kernel) {
  
  long int no_of_nodes=igraph_vcount(graph);
  igraph_vector_t indegree;
  igraph_vector_t neis;
  
  long int node;
  long int i;
  
  IGRAPH_VECTOR_INIT_FINALLY(&indegree, no_of_nodes);
  IGRAPH_VECTOR_INIT_FINALLY(&neis, 0);
  IGRAPH_CHECK(igraph_vector_resize(st, no_of_nodes));
  VECTOR(*st)[0]=VECTOR(*kernel)[0];
  
  for (node=1; node<no_of_nodes; node++) {

    IGRAPH_ALLOW_INTERRUPTION();
    
    /* new node */
    VECTOR(*st)[node]=VECTOR(*st)[node-1]+VECTOR(*kernel)[0];
    
    /* outgoing edges */
    IGRAPH_CHECK(igraph_neighbors(graph, &neis, node, IGRAPH_OUT));
    for (i=0; i<igraph_vector_size(&neis); i++) {
      long int to=VECTOR(neis)[i];
      long int xidx=VECTOR(indegree)[to];
      VECTOR(indegree)[to]+=1;
      VECTOR(*st)[node] += -VECTOR(*kernel)[xidx]+VECTOR(*kernel)[xidx+1];
    }
  }
  
  igraph_vector_destroy(&neis);
  igraph_vector_destroy(&indegree);
  IGRAPH_FINALLY_CLEAN(2);
  
  return 0;
}

int igraph_evolver_exp_d(const igraph_t *graph,
			 igraph_vector_t *expected,
			 const igraph_vector_t *kernel,
			 const igraph_vector_t *st,
			 igraph_integer_t pmaxind) {

  long int classes=pmaxind+1;
  
  igraph_vector_t ntk;
  igraph_vector_t cumst;
  igraph_vector_t ch;
  igraph_vector_t indegree;
  igraph_vector_t outdegree;
  igraph_vector_t neis;
  
  long int no_of_nodes=igraph_vcount(graph);
  long int node, i;

  IGRAPH_VECTOR_INIT_FINALLY(&ntk, classes);
  IGRAPH_VECTOR_INIT_FINALLY(&ch, classes);
  IGRAPH_VECTOR_INIT_FINALLY(&cumst, no_of_nodes+1);
  IGRAPH_VECTOR_INIT_FINALLY(&indegree, no_of_nodes);
  IGRAPH_VECTOR_INIT_FINALLY(&neis, 0);
  IGRAPH_VECTOR_INIT_FINALLY(&outdegree, no_of_nodes);
  
  IGRAPH_CHECK(igraph_degree(graph, &outdegree, igraph_vss_all(),
			     IGRAPH_OUT, IGRAPH_LOOPS));
  
  /* create the cumulative sum of dt/S(t) */
  VECTOR(cumst)[0]=0;
  for (i=0; i<no_of_nodes; i++) {
    VECTOR(cumst)[i+1]=VECTOR(cumst)[i] +
      VECTOR(outdegree)[i]/VECTOR(*st)[i];
  }
  
  igraph_vector_destroy(&outdegree);
  IGRAPH_FINALLY_CLEAN(1);
  
  IGRAPH_CHECK(igraph_vector_resize(expected, classes));
  igraph_vector_null(expected);
  
  for (node=0; node<no_of_nodes; node++) {
    
    IGRAPH_ALLOW_INTERRUPTION();
    
    IGRAPH_CHECK(igraph_neighbors(graph, &neis, node, IGRAPH_OUT));
    
    /* update degree and ntk, and result when needed */
    for (i=0; i<igraph_vector_size(&neis); i++) {
      long int to=VECTOR(neis)[i];
      long int xidx=VECTOR(indegree)[to];
      VECTOR(indegree)[to]++;
      
      VECTOR(ntk)[xidx]--;
      VECTOR(*expected)[xidx] += (VECTOR(ntk)[xidx]+1)*
	(VECTOR(cumst)[node]-VECTOR(cumst)[(long int)VECTOR(ch)[xidx]]);
      VECTOR(ch)[xidx]=node;

      VECTOR(ntk)[xidx+1]++;
      VECTOR(*expected)[xidx+1] += (VECTOR(ntk)[xidx+1]-1)*
	(VECTOR(cumst)[node]-VECTOR(cumst)[(long int)VECTOR(ch)[xidx+1]]);
      VECTOR(ch)[xidx+1]=node;
    }
    
    VECTOR(ntk)[0]++;
    VECTOR(*expected)[0] += (VECTOR(ntk)[0]-1)*
      (VECTOR(cumst)[node]-VECTOR(cumst)[(long int)VECTOR(ch)[0]]);
    VECTOR(ch)[0]=node;
  }
  
  /* complete res */
  for (i=0; i<classes; i++) {
    VECTOR(*expected)[i] += VECTOR(ntk)[i]*
      (VECTOR(cumst)[node]-VECTOR(cumst)[(long int)VECTOR(ch)[i]]);
    VECTOR(*expected)[i] *= VECTOR(*kernel)[i];
  }
  
  igraph_vector_destroy(&neis);
  igraph_vector_destroy(&indegree);
  igraph_vector_destroy(&cumst);
  igraph_vector_destroy(&ch);
  igraph_vector_destroy(&ntk);
  IGRAPH_FINALLY_CLEAN(5);
    
  return 0;
}

int igraph_evolver_error_d(const igraph_t *graph,
			   const igraph_vector_t *kernel,
			   const igraph_vector_t *st,
			   igraph_integer_t maxind,
			   igraph_real_t *logprob,
			   igraph_real_t *lognull) {

  long int classes=maxind+1;
  long int no_of_nodes=igraph_vcount(graph);
  igraph_vector_t indegree;
  igraph_vector_t neis;
  igraph_vector_t ntk;
  
  long int node;
  long int i;

  igraph_real_t rlogprob, rlognull, *mylogprob=logprob, *mylognull=lognull;
  
  IGRAPH_VECTOR_INIT_FINALLY(&ntk, classes);
  IGRAPH_VECTOR_INIT_FINALLY(&indegree, no_of_nodes);
  IGRAPH_VECTOR_INIT_FINALLY(&neis, 0);

  if (!mylogprob) { mylogprob=&rlogprob; }
  if (!mylognull) { mylognull=&rlognull; }

  *mylogprob=0;
  *mylognull=0;
  
  for (node=0; node<no_of_nodes; node++) {
    
    IGRAPH_ALLOW_INTERRUPTION();
    
    IGRAPH_CHECK(igraph_neighbors(graph, &neis, node, IGRAPH_OUT));
    for (i=0; i<igraph_vector_size(&neis); i++) {
      long int to=VECTOR(neis)[i];
      long int xidx=VECTOR(indegree)[to];
      
      igraph_real_t prob=VECTOR(*kernel)[xidx]*VECTOR(ntk)[xidx]/VECTOR(*st)[node-1];
      igraph_real_t nullprob=VECTOR(ntk)[xidx]/node;
      
      *mylogprob += log(prob);
      *mylognull += log(nullprob);
    }
    
    /* update */
    for (i=0; i<igraph_vector_size(&neis); i++) {
      long int to=VECTOR(neis)[i];
      long int xidx=VECTOR(indegree)[to];
      
      VECTOR(indegree)[to] += 1;
      VECTOR(ntk)[xidx] -= 1;
      VECTOR(ntk)[xidx+1] += 1;
    }
    VECTOR(ntk)[0] += 1;
  }
  
  igraph_vector_destroy(&neis);
  igraph_vector_destroy(&indegree);
  igraph_vector_destroy(&ntk);
  IGRAPH_FINALLY_CLEAN(3);
  
  return 0;
} 
  
/***********************************************/
/* in-degree, age                              */
/***********************************************/

int igraph_evolver_ad(const igraph_t *graph,
		      igraph_integer_t niter,
		      igraph_integer_t agebins,
		      igraph_matrix_t *kernel,
		      igraph_matrix_t *sd,
		      igraph_matrix_t *norm,
		      igraph_matrix_t *cites,
		      igraph_matrix_t *expected,
		      igraph_real_t *logprob,
		      igraph_real_t *lognull,
		      const igraph_matrix_t *debug,
		      igraph_vector_ptr_t *debugres) {

  long int no_of_nodes=igraph_vcount(graph);
  igraph_vector_t st;
  long int i;
  igraph_integer_t maxdegree;
  
  IGRAPH_VECTOR_INIT_FINALLY(&st, no_of_nodes);
  for (i=0; i<no_of_nodes; i++) {
    VECTOR(st)[i]=1;
  }

  IGRAPH_CHECK(igraph_maxdegree(graph, &maxdegree, igraph_vss_all(),
				IGRAPH_IN, IGRAPH_LOOPS));
  
  for (i=0; i<niter; i++) {

    IGRAPH_ALLOW_INTERRUPTION();
    
    if (i+1 != niter) {		/* not the last iteration */
      /* measure */
      IGRAPH_CHECK(igraph_evolver_mes_ad(graph, kernel, 0 /*sd*/, 0 /*norm*/,
					 0 /*cites*/, 0 /*debug*/, 0 /*debugres*/,
					 &st, maxdegree, agebins));
      
      /* normalize */
      igraph_matrix_multiply(kernel, 1/igraph_matrix_sum(kernel));
      
      /* update st */
      IGRAPH_CHECK(igraph_evolver_st_ad(graph, &st, kernel));
    } else {
      /* measure */
      IGRAPH_CHECK(igraph_evolver_mes_ad(graph, kernel, sd, norm, cites, debug,
					 debugres, &st, maxdegree, agebins));
      
      /* normalize */
      igraph_matrix_multiply(kernel, 1/igraph_matrix_sum(kernel));
      
      /* update st */
      IGRAPH_CHECK(igraph_evolver_st_ad(graph, &st, kernel));

      /* expected number of citations */
      if (expected) {
	IGRAPH_CHECK(igraph_evolver_exp_ad(graph, expected, kernel, &st,
					   maxdegree, agebins));
      }

      /* error calculation */
      if (logprob || lognull) {
	IGRAPH_CHECK(igraph_evolver_error_ad(graph, kernel, &st, maxdegree,
					     agebins, logprob, lognull));
      }
    }

  }
  
  igraph_vector_destroy(&st);
  IGRAPH_FINALLY_CLEAN(1);
  
  return 0;
}

int igraph_evolver_mes_ad(const igraph_t *graph,
			  igraph_matrix_t *kernel,
			  igraph_matrix_t *sd,
			  igraph_matrix_t *norm,
			  igraph_matrix_t *cites,
			  const igraph_matrix_t *debug,
			  igraph_vector_ptr_t *debugres,
			  const igraph_vector_t *st,
			  igraph_integer_t pmaxind,
			  igraph_integer_t pagebins) {
  
  long int maxind=pmaxind, agebins=pagebins;
  long int no_of_nodes=igraph_vcount(graph);
  long int binwidth=no_of_nodes/agebins+1;
  
  igraph_vector_t indegree;
  igraph_matrix_t ntkl, ch, v_normfact, *normfact, v_notnull, *notnull;

  igraph_vector_t neis;

  long int node, i, j, k;
  long int edges=0;

  IGRAPH_VECTOR_INIT_FINALLY(&indegree, no_of_nodes);
  IGRAPH_MATRIX_INIT_FINALLY(&ntkl, maxind+1, agebins+1);
  IGRAPH_MATRIX_INIT_FINALLY(&ch, maxind+1, agebins+1);
  IGRAPH_VECTOR_INIT_FINALLY(&neis, 0);

  if (norm) {
    normfact=norm;
    IGRAPH_CHECK(igraph_matrix_resize(normfact, maxind+1, agebins));
    igraph_matrix_null(normfact);
  } else {
    normfact=&v_normfact;
    IGRAPH_MATRIX_INIT_FINALLY(normfact, maxind+1, agebins);
  }
  if (cites) {
    notnull=cites;
    IGRAPH_CHECK(igraph_matrix_resize(notnull, maxind+1, agebins));
    igraph_matrix_null(notnull);
  } else {
    notnull=&v_notnull;
    IGRAPH_MATRIX_INIT_FINALLY(notnull, maxind+1, agebins);
  }
  
  IGRAPH_CHECK(igraph_matrix_resize(kernel, maxind+1, agebins));
  igraph_matrix_null(kernel);
  if (sd) {
    IGRAPH_CHECK(igraph_matrix_resize(sd, maxind+1, agebins));
    igraph_matrix_null(sd);
  }  

  if (binwidth>1) {
    MATRIX(ntkl, 0, 0)=1;
  } else {
    MATRIX(ntkl, 0, 1)=1;
  }

  for (node=0; node<no_of_nodes-1; node++) {
    
    IGRAPH_ALLOW_INTERRUPTION();
    
    /* Estimate A() */
    IGRAPH_CHECK(igraph_neighbors(graph, &neis, node+1, IGRAPH_OUT));
    for (i=0; i<igraph_vector_size(&neis); i++) {
      long int to=VECTOR(neis)[i];
      long int xidx=VECTOR(indegree)[to];
      long int yidx=(node+1-to)/binwidth;
      
      double xk=VECTOR(*st)[node]/MATRIX(ntkl, xidx, yidx);
      double oldm=MATRIX(*kernel, xidx, yidx);      
      MATRIX(*notnull, xidx, yidx) += 1;
      MATRIX(*kernel, xidx, yidx) += (xk-oldm)/MATRIX(*notnull, xidx, yidx);
      if (sd) {
	MATRIX(*sd, xidx, yidx) += (xk-oldm)*(xk-MATRIX(*kernel, xidx, yidx));
      }
      /* TODO: debug */
    }
    
    /* Update ntkl & co */
    edges += igraph_vector_size(&neis);
    for (i=0; i<igraph_vector_size(&neis); i++) {
      long int to=VECTOR(neis)[i];
      long int xidx=VECTOR(indegree)[to];
      long int yidx=(node+1-to)/binwidth;
      
      VECTOR(indegree)[to] += 1;
      MATRIX(ntkl, xidx, yidx) -= 1;
      if (MATRIX(ntkl, xidx, yidx)==0) {
	MATRIX(*normfact, xidx, yidx) += (edges-MATRIX(ch, xidx, yidx));
      }
      MATRIX(ntkl, xidx+1, yidx) += 1;
      if (MATRIX(ntkl, xidx+1, yidx)==1) {
	MATRIX(ch, xidx+1, yidx)=edges;
      }
    }
    /* new node */
    MATRIX(ntkl, 0, 0) += 1;
    if (MATRIX(ntkl, 0, 0)==1) {
      MATRIX(ch, 0, 0)=edges;
    }
    /* aging */
    for (k=1; node+1-binwidth*k+1>=0; k++) {
      long int shnode=node+1-binwidth*k+1;
      long int deg=VECTOR(indegree)[shnode];
      MATRIX(ntkl, deg, k-1)--;
      if (MATRIX(ntkl, deg, k-1)==0) {
	MATRIX(*normfact, deg, k-1) += (edges-MATRIX(ch, deg, k-1));
      }
      MATRIX(ntkl, deg, k) += 1;
      if (MATRIX(ntkl, deg, k)==1) {
	MATRIX(ch, deg, k)=edges;
      }
    }
  }
  
  /* Make normfact up to date, calculate mean, sd */
  for (i=0; i<maxind+1; i++) {
    for (j=0; j<agebins; j++) {
      igraph_real_t oldmean;
      if (MATRIX(ntkl, i, j) != 0) {
	MATRIX(*normfact, i, j) += (edges-MATRIX(ch, i, j));
      }
      if (MATRIX(*normfact, i, j)==0) {
	MATRIX(*kernel, i, j)=0;
	MATRIX(*normfact, i, j)=1;
      }
      oldmean=MATRIX(*kernel, i, j);
      MATRIX(*kernel, i, j) *= MATRIX(*notnull, i, j)/MATRIX(*normfact, i, j);
      if (sd) {
	MATRIX(*sd, i, j) +=
	  oldmean * oldmean * MATRIX(*notnull, i, j) * 
	  (1-MATRIX(*notnull, i, j)/MATRIX(*normfact, i, j));
	MATRIX(*sd, i, j) = sqrt(MATRIX(*sd, i, j)/(MATRIX(*normfact, i, j)-1));
      }
    }
  }
  
  if (!cites) {
    igraph_matrix_destroy(notnull);
    IGRAPH_FINALLY_CLEAN(1);
  }
  if (!norm) {
    igraph_matrix_destroy(normfact);
    IGRAPH_FINALLY_CLEAN(1);
  }
  igraph_vector_destroy(&neis);
  igraph_matrix_destroy(&ch);
  igraph_matrix_destroy(&ntkl);
  igraph_vector_destroy(&indegree);
  IGRAPH_FINALLY_CLEAN(4);

  return 0;
}

int igraph_evolver_st_ad(const igraph_t *graph,
			 igraph_vector_t *st,
			 const igraph_matrix_t *kernel) {
  long int agebins=igraph_matrix_ncol(kernel);
  long int no_of_nodes=igraph_vcount(graph);
  long int binwidth=no_of_nodes/agebins+1;
  
  igraph_vector_t indegree;
  igraph_vector_t neis;
  
  long int node, i, k;  
  
  IGRAPH_VECTOR_INIT_FINALLY(&indegree, no_of_nodes);
  IGRAPH_VECTOR_INIT_FINALLY(&neis, 0);
  IGRAPH_CHECK(igraph_vector_resize(st, no_of_nodes));
  if (binwidth>1) {
    VECTOR(*st)[0]=MATRIX(*kernel, 0, 0);
  } else {
    VECTOR(*st)[0]=MATRIX(*kernel, 0, 1);
  }
  
  for (node=1; node<no_of_nodes; node++) {
    
    IGRAPH_ALLOW_INTERRUPTION();
    
    /* new node */
    VECTOR(*st)[node]=VECTOR(*st)[node-1] + MATRIX(*kernel, 0, 0);
    
    /* outgoing edges */
    IGRAPH_CHECK(igraph_neighbors(graph, &neis, node, IGRAPH_OUT));
    for (i=0; i<igraph_vector_size(&neis); i++) {
      long int to=VECTOR(neis)[i];
      long int xidx=VECTOR(indegree)[to];
      long int yidx=(node-to)/binwidth;
      VECTOR(indegree)[to] += 1;
      VECTOR(*st)[node] +=
	-MATRIX(*kernel, xidx, yidx)+MATRIX(*kernel, xidx+1, yidx);
    }

    /* aging */
    for (k=1; node-binwidth*k+1 >= 0; k++) {
      long int shnode=node-binwidth*k+1;
      long int deg=VECTOR(indegree)[shnode];
      VECTOR(*st)[node] += -MATRIX(*kernel, deg, k-1)+MATRIX(*kernel, deg, k);
    }    
    
  }  
  
  igraph_vector_destroy(&neis);
  igraph_vector_destroy(&indegree);
  IGRAPH_FINALLY_CLEAN(2);

  return 0;
}

int igraph_evolver_exp_ad(const igraph_t *graph,
			  igraph_matrix_t *expected,
			  const igraph_matrix_t *kernel,
			  const igraph_vector_t *st,
			  igraph_integer_t pmaxind,
			  igraph_integer_t pagebins) {
  
  long int maxind=pmaxind, agebins=pagebins;
  long int no_of_nodes=igraph_vcount(graph);
  long int binwidth=no_of_nodes/agebins+1;
  
  igraph_vector_t indegree;
  igraph_vector_t outdegree;
  igraph_vector_t cumst;
  igraph_matrix_t ntkl;
  igraph_matrix_t ch;
  igraph_vector_t neis;

  long int node, i, j, k;
  
  IGRAPH_MATRIX_INIT_FINALLY(&ntkl, maxind+1, agebins);
  IGRAPH_MATRIX_INIT_FINALLY(&ch, maxind+1, agebins);
  IGRAPH_VECTOR_INIT_FINALLY(&cumst, no_of_nodes+1);
  IGRAPH_VECTOR_INIT_FINALLY(&indegree, no_of_nodes);
  IGRAPH_VECTOR_INIT_FINALLY(&neis, 0);
  IGRAPH_VECTOR_INIT_FINALLY(&outdegree, no_of_nodes);
  
  IGRAPH_CHECK(igraph_degree(graph, &outdegree, igraph_vss_all(), 
			     IGRAPH_OUT, IGRAPH_LOOPS));
  
  /* create cumulative sum of dt/S(t) */
  VECTOR(cumst)[0]=0;
  for (i=0; i<no_of_nodes; i++) {
    VECTOR(cumst)[i+1]=VECTOR(cumst)[i] +
      VECTOR(outdegree)[i]/VECTOR(*st)[i];
  }

  igraph_vector_destroy(&outdegree);
  IGRAPH_FINALLY_CLEAN(1);
  
  IGRAPH_CHECK(igraph_matrix_resize(expected, maxind+1, agebins));
  igraph_matrix_null(expected);
  
  for (node=0; node<no_of_nodes; node++) {
    
    IGRAPH_ALLOW_INTERRUPTION();
    
    /* update degree and ntk, and result when needed */
    IGRAPH_CHECK(igraph_neighbors(graph, &neis, node, IGRAPH_OUT));
    for (i=0; i<igraph_vector_size(&neis); i++) {
      long int to=VECTOR(neis)[i];
      long int xidx=VECTOR(indegree)[to];
      long int yidx=(node-to)/binwidth;
      
      VECTOR(indegree)[to] += 1;
      
      MATRIX(ntkl, xidx, yidx) -= 1;
      MATRIX(*expected, xidx, yidx) += (MATRIX(ntkl, xidx, yidx)+1) *
	(VECTOR(cumst)[node]-VECTOR(cumst)[(long int)MATRIX(ch, xidx, yidx)]);
      MATRIX(ch, xidx, yidx)=node;
      
      MATRIX(ntkl, xidx+1, yidx) += 1;
      MATRIX(*expected, xidx+1, yidx) += (MATRIX(ntkl, xidx+1, yidx)-1) *
	(VECTOR(cumst)[node]-VECTOR(cumst)[(long int)MATRIX(ch, xidx+1, yidx)]);
      MATRIX(ch, xidx+1, yidx)=node;
    }
    /* new node */
    MATRIX(ntkl, 0, 0) += 1;
    MATRIX(*expected, 0, 0) += (MATRIX(ntkl, 0, 0)-1)*
      (VECTOR(cumst)[node]-VECTOR(cumst)[(long int)MATRIX(ch, 0, 0)]);
    MATRIX(ch, 0, 0)=node;
    /* aging */
    for (k=1; node-binwidth*k+1>=0; k++) {
      long int shnode=node-binwidth*k+1;
      long int deg=VECTOR(indegree)[shnode];
      MATRIX(ntkl, deg, k-1) -= 1;
      MATRIX(*expected, deg, k-1) += (MATRIX(ntkl, deg, k-1)+1) *
	(VECTOR(cumst)[node]-VECTOR(cumst)[(long int)MATRIX(ch, deg, k-1)]);
      MATRIX(ch, deg, k-1)=node;
      MATRIX(ntkl, deg, k) += 1;
      MATRIX(*expected, deg, k) += (MATRIX(ntkl, deg, k)-1) *
	(VECTOR(cumst)[node]-VECTOR(cumst)[(long int)MATRIX(ch, deg, k)]);
      MATRIX(ch, deg, k)=node;
    }
    
  }

  /* complete res */
  for (i=0; i<maxind+1; i++) {
    for (j=0; j<agebins; j++) {
      MATRIX(*expected, i, j) += MATRIX(ntkl, i, j) *
	(VECTOR(cumst)[node]-VECTOR(cumst)[(long int)MATRIX(ch, i, j)]);
      MATRIX(*expected, i, j) *= MATRIX(*kernel, i, j);
    }
  }
  
  igraph_vector_destroy(&neis);
  igraph_vector_destroy(&indegree);
  igraph_vector_destroy(&cumst);
  igraph_matrix_destroy(&ch);
  igraph_matrix_destroy(&ntkl);
  IGRAPH_FINALLY_CLEAN(5);

  return 0;
}

int igraph_evolver_error_ad(const igraph_t *graph, 
			    const igraph_matrix_t *kernel,
			    const igraph_vector_t *st,
			    igraph_integer_t pmaxind,
			    igraph_integer_t pagebins,
			    igraph_real_t *logprob,
			    igraph_real_t *lognull) {
  
  long int maxind=pmaxind, agebins=pagebins;
  long int no_of_nodes=igraph_vcount(graph);
  long int binwidth=no_of_nodes/agebins+1;
  
  igraph_vector_t indegree;
  igraph_matrix_t ntkl;
  
  igraph_vector_t neis;
  
  long int node, i, k;

  igraph_real_t rlogprob, rlognull, *mylogprob=logprob, *mylognull=lognull;
  
  IGRAPH_VECTOR_INIT_FINALLY(&indegree, no_of_nodes);
  IGRAPH_MATRIX_INIT_FINALLY(&ntkl, maxind+1, agebins+1);
  IGRAPH_VECTOR_INIT_FINALLY(&neis, 0);
						    
  if (binwidth>1) {
    MATRIX(ntkl, 0, 0)=1;
  } else {
    MATRIX(ntkl, 0, 1)=1;
  }

  if (!logprob) { mylogprob=&rlogprob; }
  if (!lognull) { mylognull=&rlognull; }

  *mylogprob=0;
  *mylognull=0;
  
  for (node=0; node<no_of_nodes-1; node++) {
    
    IGRAPH_ALLOW_INTERRUPTION();
    
    IGRAPH_CHECK(igraph_neighbors(graph, &neis, node+1, IGRAPH_OUT));
    for (i=0; i<igraph_vector_size(&neis); i++) {
      long int to=VECTOR(neis)[i];
      long int xidx=VECTOR(indegree)[to];
      long int yidx=(node+1-to)/binwidth;
      
      igraph_real_t prob=MATRIX(*kernel, xidx, yidx)*MATRIX(ntkl, xidx, yidx) /
	VECTOR(*st)[node];
      igraph_real_t nullprob=MATRIX(ntkl, xidx, yidx)/(node+1);
      
      *mylogprob += log(prob);
      *mylognull += log(nullprob);
    }

    /* update */
    for (i=0; i<igraph_vector_size(&neis); i++) {
      long int to=VECTOR(neis)[i];
      long int xidx=VECTOR(indegree)[to];
      long int yidx=(node+1-to)/binwidth;
      
      VECTOR(indegree)[to] += 1;
      MATRIX(ntkl, xidx, yidx) -= 1;
      MATRIX(ntkl, xidx+1, yidx) += 1;
    }
    MATRIX(ntkl, 0, 0) += 1;
    /* aging */
    for (k=1; node+1-binwidth*k+1>=0; k++) {
      long int shnode=node+1-binwidth*k+1;
      long int deg=VECTOR(indegree)[shnode];
      MATRIX(ntkl, deg, k-1) -= 1;
      MATRIX(ntkl, deg, k) += 1;
    }
    
  }
  
  igraph_vector_destroy(&neis);
  igraph_matrix_destroy(&ntkl);
  igraph_vector_destroy(&indegree);
  IGRAPH_FINALLY_CLEAN(3);
  
  return 0;
}
