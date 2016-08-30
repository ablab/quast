//Copyright (c) 2003 by  Mihaela Pertea.


/****************************************************************/
/****************************************************************/
/* File Name :	tree_util_prob.c				*/
/* Contains modules : 	read_tree				*/
/*			read_subtree				*/
/*			read_hp					*/
/*			read_header				*/
/*			isleftchild				*/
/*			isrightchild				*/
/* Uses modules in :	oc1.h					*/
/*			util.c					*/ 
/* Is used by modules in :	substringscores.c		*/
/* Remarks       : 	These routines are mainly used to read	*/
/*			a decision tree from a file, and to     */
/*                      write a tree to a file.			*/
/****************************************************************/		
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include "oc1.h"

extern int no_of_dimensions, no_of_categories;

struct tree_node *extra_node;
char train_data[LINESIZE];


/* prototypes */
struct tree_node *read_tree(char *);
void read_subtree(struct tree_node *, FILE *);
struct tree_node *read_hp(FILE *);
int read_header(FILE *);
int isleftchild(struct tree_node *, struct tree_node *);
int isrightchild(struct tree_node *x, struct tree_node *y);

     
/************************************************************************/
/* Module name :	read_tree					*/ 
/* Functionality : High level routine for reading in a decision tree	*/
/* Parameters :	decision_tree : Name of the file in which the tree is	*/
/*		stored.							*/
/* Returns :	pointer to the root node of the tree.			*/
/* Calls modules :	read_subtree					*/
/*			read_header					*/
/*			read_hp						*/
/*			error (util.c)					*/
/* Is called by modules :	main (mktree.c)				*/
/*				main (gen_data.c)			*/
/* Remarks : 	It is assumed that the file "decision_tree" is		*/
/* 		written in a format similar to the output of the	*/
/*		write_tree module. A sample decision tree is given in   */
/*              the file sample.dt.				        */ 
/************************************************************************/
struct tree_node *read_tree(char *decision_tree)
{
  FILE *dtree;
  struct tree_node *root;

  if ((dtree = fopen(decision_tree,"r")) == NULL)
    error("Decision Tree file can not be opened.");
  
  if ( !(read_header(dtree))) 
    error("Decision tree invalid/absent.");
  
  if ((root = read_hp(dtree)) == NULL)
    error("Decision tree invalid/absent.");
  
  root->parent = NULL;
  extra_node = NULL;
  read_subtree(root,dtree);
  
  fclose(dtree);
  return(root);
}


/************************************************************************/
/* Module name :	read_subtree					*/ 
/* Functionality :	recursively reads in the hyperplane, left 	*/
/*			subtree and the right subtree at a node of 	*/
/*			the decision tree. 				*/
/* Parameters :	root : node, the subtree at which is to be read.	*/
/*		dtree: file pointer where the tree is available.	*/
/* Returns :	nothing.						*/
/* Calls modules :	read_subtree					*/
/*			read_hp						*/
/*			isleftchild					*/
/*			isrightchild					*/
/* Is called by modules :	read_tree				*/
/*				read_subtree				*/
/* Important Variables Used :	extra_node 				*/
/*	Hyperplanes are read from the file "dtree" in the order "parent,*/
/*	left child, right child". In case a node does not have either a	*/
/* 	left child or a right child or both, this routine reads one 	*/
/*	hyperplane before it is needed. Such hyperplanes, that are read	*/
/*	before they are needed, are stored in extra_node.		*/
/************************************************************************/
void read_subtree(struct tree_node *root, FILE *dtree)
{
  struct tree_node *cur_node;
  
  if (extra_node != NULL)
    {
      cur_node = extra_node;
      extra_node = NULL;
    }
  else cur_node = read_hp(dtree);
  
  if (cur_node == NULL) return;
  if (isleftchild(cur_node,root))
    {
      cur_node->parent = root;
      root->left = cur_node;
      
      read_subtree(cur_node,dtree);
      if (extra_node != NULL)
	{
	  cur_node = extra_node;
	  extra_node = NULL;
	}
      else
	cur_node = read_hp(dtree);
      if (cur_node == NULL) return;
    }

  if (isrightchild(cur_node,root))
    {
      cur_node->parent = root;
      root->right = cur_node;
      read_subtree(cur_node,dtree);
    }
  else extra_node = cur_node;
}

/************************************************************************/
/* Module name : read_hp						*/
/* Functionality :	Reads a hyperplane (one node of the decision	*/
/*			tree).						*/
/* Parameters :	dtree : file pointer to the decision tree file.		*/
/* Returns : pointer to the decision tree node read.			*/
/* Calls modules :	vector (util.c)					*/
/*			error (util.c)					*/
/* Is called by modules :	read_tree				*/
/*				read_subtree				*/
/* Remarks :	Rather strict adherance to format.			*/
/*		Please carefully follow the format in sample.dt, if	*/
/*		your decision tree files are not produced by "mktree".	*/
/************************************************************************/
struct tree_node *read_hp(FILE *dtree)
{
  struct tree_node *cur_node;
  float temp;
  char c;
  int i;

  cur_node = (struct tree_node *)malloc(sizeof(struct tree_node));
  cur_node->coefficients = vector(0,no_of_dimensions+1);
  cur_node->left_count = ivector(0,no_of_categories);
  cur_node->right_count = ivector(0,no_of_categories);
  
  for (i=1;i<=no_of_dimensions+1;i++) cur_node->coefficients[i-1] = 0;
  
  cur_node->left = cur_node->right = NULL;
  
  while (isspace(c = getc(dtree)));
  ungetc(c,dtree); 
  
  if (fscanf(dtree,"%[^' '] Hyperplane: Left = [", cur_node->label) != 1)
    return(NULL);

  for (i=1;i<no_of_categories;i++)
    if (fscanf(dtree,"%d,",&cur_node->left_count[i-1]) != 1)
      return(NULL); 
  if (fscanf(dtree,"%d], Right = [",
	     &cur_node->left_count[no_of_categories-1]) != 1)
    return(NULL); 
  for (i=1;i<no_of_categories;i++)
    if (fscanf(dtree,"%d,",&cur_node->right_count[i-1]) != 1)
      return(NULL); 
  if (fscanf(dtree,"%d]\n", &cur_node->right_count[no_of_categories-1]) != 1)
    return(NULL); 

  if (!strcmp(cur_node->label,"Root")) strcpy(cur_node->label,"");
  
  while (TRUE)
    {
      if ((fscanf(dtree,"%f %c",&temp,&c)) != 2)
	error("Invalid/Absent hyperplane equation.");
      if (c == 'x')
	{ 
	  if ((fscanf(dtree,"[%d] +",&i)) != 1) 
	    error("Read-Hp: Invalid hyperplane equation.");
	  if (i <= 0 || i > no_of_dimensions+1) 
	    error("Read_Hp: Invalid coefficient index in decision tree.");
	  cur_node->coefficients[i-1] = temp;
	}
      else if (c == '=')
	{
	  fscanf(dtree," 0\n\n");
	  cur_node->coefficients[no_of_dimensions] = temp;
	  break;
	}
    }

  cur_node->no_of_points = cur_node->left_total = cur_node->right_total = 0;
  cur_node->left_cat = cur_node->right_cat = 1;
  for (i=1;i<=no_of_categories;i++)
    {
      cur_node->left_total += cur_node->left_count[i-1];
      cur_node->right_total += cur_node->right_count[i-1];
      if(cur_node->left_count[i-1] > cur_node->left_count[(cur_node->left_cat)-1])
	cur_node->left_cat = i;
      if(cur_node->right_count[i-1] > cur_node->right_count[(cur_node->right_cat)-1])
	cur_node->right_cat = i;
    }
  cur_node->no_of_points =
    cur_node->left_total + cur_node->right_total;
  
  return(cur_node);
}

/************************************************************************/
/* Module name : isleftchild						*/
/* Functionality : 	Checks if node x is a left child of node y.	*/
/*			i.e., checks if the label of node x is the same	*/
/*			as label of y, concatenated with "l".		*/
/* Parameters : x,y : pointers to two decision tree nodes.		*/
/* Returns :	1 : if x is the left child of y				*/
/*		0 : otherwise						*/
/* Is called by modules :	read_subtree				*/
/************************************************************************/
int isleftchild(struct tree_node *x, struct tree_node *y)
{
  char temp[MAX_DT_DEPTH];
  
  strcpy(temp,y->label);
  if (!strcmp(strcat(temp,"l"),x->label)) return(1);
  else return(0);
}

/************************************************************************/
/* Module name : isrightchild						*/
/* Functionality : 	Checks if node x is a right child of node y.	*/
/*			i.e., checks if the label of node x is the same	*/
/*			as label of y, concatenated with "l".		*/
/* Parameters : x,y : pointers to two decision tree nodes.		*/
/* Returns :	1 : if x is the right child of y			*/
/*		0 : otherwise						*/
/* Is called by modules :	read_subtree				*/
/************************************************************************/
int isrightchild(struct tree_node *x, struct tree_node *y)
{
  char temp[MAX_DT_DEPTH];
  
  strcpy(temp,y->label);
  if (!strcmp(strcat(temp,"r"),x->label)) return(1);
  else return(0);
}

/************************************************************************/
/* Module name : read_header						*/
/* Functionality :	Reads the header information in a decision tree	*/
/*			file.						*/
/* Parameters :	dtree : file pointer to the decision tree file.		*/
/* Returns : 	1 : if the header is successfully read.			*/
/*		0 : otherwise.						*/
/* Calls modules : none.						*/
/* Is called by modules :	read_tree				*/
/* Remarks :	Rather strict adherance to format.			*/
/*		Please carefully follow the format in sample.dt, if	*/
/*		your decision tree files are not produced by "mktree".	*/
/************************************************************************/
int read_header(FILE *dtree)
{
  if ((fscanf(dtree,"Training set: %[^,], ",train_data)) != 1) return(0);
  if ((fscanf(dtree,"Dimensions: %d, Categories: %d\n",
	      &no_of_dimensions,&no_of_categories)) != 2) return(0);
  return(1);
}

/************************************************************************/
/************************************************************************/
