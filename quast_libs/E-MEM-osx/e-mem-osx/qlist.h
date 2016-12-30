/* ================================================================= *
 *  qlist.h : Header file with supporting class definitions          *
 *                                                                   *
 *  E-MEM: An efficient (MUMmer-like) tool to retrieve Maximum Exact *
 *         Matches using hashing based algorithm                     *
 *                                                                   *
 *  Copyright (c) 2014, Nilesh Khiste                                *
 *  All rights reserved                                              *
 *                                                                   * 
 *  This program is free software: you can redistribute it and/or    *
 *  modify it under the terms of the GNU General Public License as   *
 *  published by the Free Software Foundation, either version 3 of   *
 *  the License, or (at your option) any later version.              *
 *                                                                   *
 *  This program is distributed in the hope that it will be useful,  *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of   *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the    *
 *  GNU General Public License for more details.                     *
 *                                                                   *
 *  You should have received a copy of the GNU General Public        *
 *  License along with this program.                                 *
 *                                                                   *
 *  This file is subject to the terms and conditions defined in the  *
 *  file 'LICENSE', which is part of this source code package.       *
 * ================================================================= */

class qneryList; 
 
class MEMS_LIST {
    friend class queryList;
    uint64_t key;
    MEMS_LIST *next;
};

class queryList { 
    uint64_t left; 
    uint64_t right; 
    class queryList* next;
    class MEMS_LIST* mems;

    queryList *queryList_Alloc (uint64_t a=0, uint64_t b=0, uint64_t key=0)
    {
        queryList *q = new queryList;
        q->left = a;
        q->right = b;
        q->next=NULL;
        q->mems=new MEMS_LIST;
        q->mems->key = key;
        q->mems->next = NULL;
        return q;
    }
  
 public:
    void ListFree(queryList** listRef)
    {
        if (!*listRef)
            return;
        while(*listRef)
        {    
            // Free all MEMs node found using this this Kmer
            MEMS_LIST *remMems = (*listRef)->mems, *remMemNext=NULL;
            while(remMems) {
                remMemNext = remMems->next;
                delete remMems;
                remMems = remMemNext;
            }
            queryList *tmp = (*listRef)->next;
            delete *listRef;
            *listRef=tmp;
        }
    }

    void ListAdd(queryList** listRef, uint64_t l, uint64_t r, uint64_t key)
    {
        queryList *prev_node=*listRef;
        queryList *node=*listRef;

        if (*listRef == NULL) {
            *listRef = queryList_Alloc (l, r, key);
            return;
        }

        while(node)
        {
            if (node->right > r) {
                queryList *p = queryList_Alloc(l, r, key);
                p->next = node;
                if (node == this)
                   *listRef = p;
                else
                   prev_node->next = p;
                return;
            }else if (node->right == r) {
                if(node->left == l){
                    //Add MEM_LIST item
                    MEMS_LIST *mems=new MEMS_LIST;
                    mems->key = key;
                    mems->next = node->mems;
                    node->mems =mems;
                    return; //node already in the list
                }
            }

            prev_node = node;
            node=node->next;
            if (!node) { // end of list
                queryList *p = queryList_Alloc(l, r, key);
                prev_node->next = p;
                return;
            }
         }
         return;
    }

    int checkRedundantMEM(queryList ** listRef, uint64_t refKmerPos, uint64_t QueryKmerPos, uint64_t totalRBits, std::unordered_multimap <uint64_t, uint64_t> &currMEMs)
    {
        // Find MEM positions from currQueryMEMs list
        queryList *p = *listRef; 
        while (p) {
            if (QueryKmerPos >= p->left && (QueryKmerPos+commonData::kmerSize-2) <= p->right) {//Found MEM
                uint64_t relLeft = QueryKmerPos - p->left;
                uint64_t relRight = p->right - QueryKmerPos;
                uint64_t refRightPos=refKmerPos+relRight;
                if(refRightPos > totalRBits)
                    refRightPos=totalRBits;
    
                if ((refKmerPos >= relLeft) ){
                    uint64_t key = ((refKmerPos-relLeft) << 32 | (refRightPos));
                    uint64_t value = ((p->left << 32) | p->right);
                    auto range = currMEMs.equal_range(key);
                    for (auto it = range.first; it != range.second; ++it) { // Element found
                        if (it->second == value)
                            return true;
                    }
                 }
            }else if ((QueryKmerPos+commonData::kmerSize-2) > p->right) {
                // Free all MEMs node found using this this Kmer
                MEMS_LIST *remMems = p->mems, *remMemNext=NULL;
                uint64_t value = ((p->left << 32) | p->right);
                p->mems = NULL;
                while(remMems) {
                    remMemNext = remMems->next;
                    auto range = currMEMs.equal_range(remMems->key);
                    for (auto it = range.first; it != range.second; ++it) { // Element found
                        if (it->second == value){
                            currMEMs.erase(it);
                            break;
                        }
                    }
                    delete remMems;
                    remMems = remMemNext;
                }
                *listRef = p->next;
                delete p;
                p = *listRef;
                continue;
            }
            p=p->next;
        }
        return false;
    }
}; 

