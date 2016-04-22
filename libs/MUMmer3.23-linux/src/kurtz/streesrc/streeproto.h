/* 
  This file is generated. Do not edit.

  A Library for the Efficient Construction and Application of Suffix Trees

  Copyright (c) 2003 by Stefan Kurtz and The Institute for
  Genomic Research.  This is OSI Certified Open Source Software.
  Please see the file LICENSE for licensing information and
  the file ACKNOWLEDGEMENTS for names of contributors to the
  code base.
*/

#ifndef STREEPROTO_H
#define STREEPROTO_H

#ifdef __cplusplus
extern "C" {
#endif

Sint constructstree(Suffixtree *stree,SYMBOL *text,Uint textlen);
Sint constructmarkmaxstree(Suffixtree *stree,SYMBOL *text,Uint textlen);
Sint constructheadstree(Suffixtree *stree,SYMBOL *text,Uint textlen,void(*processhead)(Suffixtree *,Uint,void *),void *processheadinfo);
Sint constructprogressstree(Suffixtree *stree,SYMBOL *text,Uint textlen,void (*progress)(Uint,void *),void (*finalprogress)(void *),void *info);

void freestree(Suffixtree *stree);
#ifdef __cplusplus
}
#endif

#endif
