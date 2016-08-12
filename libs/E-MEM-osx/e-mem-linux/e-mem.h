/* ================================================================= *
 *  e-mem.h : Header file for main program                           *
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

/*
 * Command line argument control
 */
#define LENGTH			0x00000001
#define REF_FILE		0x00000002
#define QUERY_FILE		0x00000004
#define SPLIT_SIZE		0x00000008
#define KMER_SIZE		0x00000010
#define NUM_THREADS		0x00000020
#define IGNORE_N		0x00000040
#define MATCH_REV		0x00000080
#define MATCH_BOTH		0x00000100
#define REL_REV_QUEPOS		0x00000200
#define FOUR_COL_OUTPUT		0x00000400
#define LEN_IN_HEADER		0x00000800

#define IS_LENGTH_DEF(x)	(x & LENGTH)
#define IS_REF_FILE_DEF(x)	(x & REF_FILE)
#define IS_QUERY_FILE_DEF(x)	(x & QUERY_FILE)
#define IS_SPLIT_SIZE_DEF(x)	(x & SPLIT_SIZE)
#define IS_KMER_SIZE_DEF(x)	(x & KMER_SIZE)
#define IS_NUM_THREADS_DEF(x)	(x & NUM_THREADS)
#define IS_IGNORE_N_DEF(x)	(x & IGNORE_N)
#define IS_MATCH_REV_DEF(x)	(x & MATCH_REV)
#define IS_MATCH_BOTH_DEF(x)	(x & MATCH_BOTH)
#define IS_RELREV_QUEPOS_DEF(x)	(x & REL_REV_QUEPOS)
#define IS_FCOL_OUTPUT_DEF(x)	(x & FOUR_COL_OUTPUT)
#define IS_LEN_IN_HEADER_DEF(x)	(x & LEN_IN_HEADER)

#define SET_LENGTH(x)		(x |= LENGTH)
#define SET_REF_FILE(x)		(x |= REF_FILE)
#define SET_QUERY_FILE(x)	(x |= QUERY_FILE)
#define SET_SPLIT_SIZE(x)	(x |= SPLIT_SIZE)
#define SET_KMER_SIZE(x)	(x |= KMER_SIZE)
#define SET_NUM_THREADS(x)	(x |= NUM_THREADS)
#define SET_IGNORE_N(x)		(x |= IGNORE_N)
#define SET_MATCH_REV(x)	(x |= MATCH_REV)
#define SET_MATCH_BOTH(x)	(x |= MATCH_BOTH)
#define SET_RELREV_QUEPOS(x)	(x |= REL_REV_QUEPOS)
#define SET_FCOL_OUTPUT(x)	(x |= FOUR_COL_OUTPUT)
#define SET_LEN_IN_HEADER(x)	(x |= LEN_IN_HEADER)

class Knode {
    uint64_t currKmer;
    uint64_t *pos;
    uint64_t getHashKey(uint64_t currKmer, uint64_t &match_found)
    {
        uint64_t key=currKmer%currHashTabSize;
        uint64_t step = 1 + (currKmer%prevHashTabSize);
        uint32_t count=1;
        while (this[key].pos != NULL) {
            if (this[key].currKmer==currKmer){ 
                match_found=1;
                return key; 
            }
            else {
                key = (key + (count*step)) % currHashTabSize; 
                count++;
            }
        }
        return key; //no collision
    }

public:
    static uint64_t currHashTabSize;
    static uint64_t prevHashTabSize;

    bool findKmer(uint64_t currKmer, uint64_t* &pos_ptr)
    {
        uint64_t match=0;
        uint64_t key=this->getHashKey(currKmer, match);
        if (match) {
           pos_ptr = this[key].pos;
           return true;
        }else
           return false;
    }
    
    void addKmerNode(uint64_t currKmer, uint64_t currKmerPos)
    {
        uint64_t i=0, match=0;
        uint64_t key=this->getHashKey(currKmer, match);
        
        if (this[key].pos==NULL) { 
            this[key].currKmer=currKmer;
            this[key].pos= new uint64_t[2];
            this[key].pos[0]= 1;  //position of the last element
            this[key].pos[1]= currKmerPos;  // Position
        }else {
            if(this[key].pos[0] & (this[key].pos[0] + 1)){
                this[key].pos[0] += 1; // Increment counter
                this[key].pos[this[key].pos[0]]= currKmerPos;  // Fill Next Position
            }else { // Increase array size
                uint64_t *r = new uint64_t[2 * (this[key].pos[0]+1)];
                r[0] = this[key].pos[0]+1;
                for (i=1; i<r[0]; i++)
                    r[i] = this[key].pos[i];    
            
                r[i] = currKmerPos;    
                delete [] this[key].pos;
                this[key].pos= r; 
            }
        }
    }
    
    Knode (uint64_t a=0, uint64_t *b=NULL)
    {
        currKmer=a;
        pos = b;
    }
    ~Knode ()
    {
        if(pos)
            delete [] pos;
    }
};

uint64_t Knode::currHashTabSize=0;
uint64_t Knode::prevHashTabSize=0;

/* Global mask array for bit manipulation */
uint64_t global_mask_right[32] = { 0x0000000000000003, /*  2 bits */ 
                                   0x000000000000000F, /*  4 bits */ 
                                   0x000000000000003F, /*  6 bits */ 
                                   0x00000000000000FF, /*  8 bits */
                                   0x00000000000003FF, /* 10 bits */
                                   0x0000000000000FFF, /* 12 bits */
                                   0x0000000000003FFF, /* 14 bits */
                                   0x000000000000FFFF, /* 16 bits */
                                   0x000000000003FFFF, /* 18 bits */
                                   0x00000000000FFFFF, /* 20 bits */
                                   0x00000000003FFFFF, /* 22 bits */
                                   0x0000000000FFFFFF, /* 24 bits */
                                   0x0000000003FFFFFF, /* 26 bits */
                                   0x000000000FFFFFFF, /* 28 bits */
                                   0x000000003FFFFFFF, /* 30 bits */
                                   0x00000000FFFFFFFF, /* 32 bits */
                                   0x00000003FFFFFFFF, /* 34 bits */
                                   0x0000000FFFFFFFFF, /* 36 bits */
                                   0x0000003FFFFFFFFF, /* 38 bits */
                                   0x000000FFFFFFFFFF, /* 40 bits */
                                   0x000003FFFFFFFFFF, /* 42 bits */
                                   0x00000FFFFFFFFFFF, /* 44 bits */
                                   0x00003FFFFFFFFFFF, /* 46 bits */
                                   0x0000FFFFFFFFFFFF, /* 48 bits */
                                   0x0003FFFFFFFFFFFF, /* 50 bits */
                                   0x000FFFFFFFFFFFFF, /* 52 bits */
                                   0x003FFFFFFFFFFFFF, /* 54 bits */
                                   0x00FFFFFFFFFFFFFF, /* 56 bits */
                                   0x03FFFFFFFFFFFFFF, /* 58 bits */
                                   0x0FFFFFFFFFFFFFFF, /* 60 bits */
                                   0x3FFFFFFFFFFFFFFF, /* 62 bits */
                                   0xFFFFFFFFFFFFFFFF, /* 64 bits */
                                  };

uint64_t global_mask_left[32] = {0xC000000000000000, /*  2 bits */ 
                                 0xF000000000000000, /*  4 bits */ 
                                 0xFC00000000000000, /*  6 bits */ 
                                 0xFF00000000000000, /*  8 bits */
                                 0xFFC0000000000000, /* 10 bits */
                                 0xFFF0000000000000, /* 12 bits */
                                 0xFFFC000000000000, /* 14 bits */
                                 0xFFFF000000000000, /* 16 bits */
                                 0xFFFFC00000000000, /* 18 bits */
                                 0xFFFFF00000000000, /* 20 bits */
                                 0xFFFFFC0000000000, /* 22 bits */
                                 0xFFFFFF0000000000, /* 24 bits */
                                 0xFFFFFFC000000000, /* 26 bits */
                                 0xFFFFFFF000000000, /* 28 bits */
                                 0xFFFFFFFC00000000, /* 30 bits */
                                 0xFFFFFFFF00000000, /* 32 bits */
                                 0xFFFFFFFFC0000000, /* 34 bits */
                                 0xFFFFFFFFF0000000, /* 36 bits */
                                 0xFFFFFFFFFC000000, /* 38 bits */
                                 0xFFFFFFFFFF000000, /* 40 bits */
                                 0xFFFFFFFFFFC00000, /* 42 bits */
                                 0xFFFFFFFFFFF00000, /* 44 bits */
                                 0xFFFFFFFFFFFC0000, /* 46 bits */
                                 0xFFFFFFFFFFFF0000, /* 48 bits */
                                 0xFFFFFFFFFFFFC000, /* 50 bits */
                                 0xFFFFFFFFFFFFF000, /* 52 bits */
                                 0xFFFFFFFFFFFFFC00, /* 54 bits */
                                 0xFFFFFFFFFFFFFF00, /* 56 bits */
                                 0xFFFFFFFFFFFFFFC0, /* 58 bits */
                                 0xFFFFFFFFFFFFFFF0, /* 60 bits */
                                 0xFFFFFFFFFFFFFFFC, /* 62 bits */
                                 0xFFFFFFFFFFFFFFFF, /* 64 bits */
                                };

/* Create and initialize array to store hash table sizes. All values are prime numbers. */
	uint64_t hashTableSize[450]={7, 13, 19, 19, 31, 61, 97, 131, 1117, 2221, 1769627, 1835027, 1900667, 1966127, 2031839, 2228483, 2359559, 2490707, 2621447, 2752679, 2883767, 3015527, 3145739, 3277283, 3408323, 3539267, 3670259, 3801143, 3932483, 4063559, 4456643, 4718699, 4980827, 5243003, 5505239, 5767187, 6029603, 6291563, 6553979, 6816527, 7079159, 7340639, 7602359, 7864799, 8126747, 8913119, 9437399, 9962207, 10485767, 11010383, 11534819, 12059123, 12583007, 13107923, 13631819, 14156543, 14680067, 15204467, 15729647, 16253423, 17825999, 18874379, 19923227, 20971799, 22020227, 23069447, 24117683, 25166423, 26214743, 27264047, 28312007, 29360147, 30410483, 31457627, 32505983, 35651783, 37749983, 39845987, 41943347, 44040383, 46137887, 48234623, 50331707, 52429067, 54526019, 56623367, 58720307, 60817763, 62915459, 65012279, 71303567, 75497999, 79691867, 83886983, 88080527, 92275307, 96470447, 100663439, 104858387, 109052183, 113246699, 117440699, 121635467, 125829239, 130023683, 142606379, 150994979, 159383759, 167772239, 176160779, 184549559, 192938003, 201327359, 209715719, 218104427, 226493747, 234882239, 243269639, 251659139, 260047367, 285215507, 301989959, 318767927, 335544323, 352321643, 369100463, 385876703, 402654059, 419432243, 436208447, 452986103, 469762067, 486539519, 503316623, 520094747, 570425399, 603979919, 637534763, 671089283, 704643287, 738198347, 771752363, 805307963, 838861103, 872415239, 905971007, 939525143, 973079279, 1006633283, 1040187419, 1140852767, 1207960679, 1275069143, 1342177379, 1409288183, 1476395699, 1543504343, 1610613119, 1677721667, 1744830587, 1811940419, 1879049087, 1946157419, 2013265967, 2080375127, 2281701827, 2415920939, 2550137039, 2684355383, 2818572539, 2952791147, 3087008663, 3221226167, 3355444187, 3489661079, 3623878823, 3758096939, 3892314659, 4026532187, 4160749883, 4563403379, 4831838783, 5100273923, 5368709219, 5637144743, 5905580687, 6174015503, 6442452119, 6710886467, 6979322123, 7247758307, 7516193123, 7784629079, 8053065599, 8321499203, 9126806147, 9663676523, 10200548819, 10737418883, 11274289319, 11811160139, 12348031523, 12884902223, 13421772839, 13958645543, 14495515943, 15032386163, 15569257247, 16106127887, 16642998803, 18253612127, 19327353083, 20401094843, 21474837719, 22548578579, 23622320927, 24696062387, 25769803799, 26843546243, 27917287907, 28991030759, 30064772327, 31138513067, 32212254947, 33285996803, 36507222923, 38654706323, 40802189423, 42949673423, 45097157927, 47244640319, 49392124247, 51539607599, 53687092307, 55834576979, 57982058579, 60129542339, 62277026327, 64424509847, 66571993199, 73014444299, 77309412407, 81604379243, 85899346727, 90194314103, 94489281203, 98784255863, 103079215439, 107374183703, 111669150239, 115964117999, 120259085183, 124554051983, 128849019059, 133143986399, 146028888179, 154618823603, 163208757527, 171798693719, 180388628579, 188978561207, 197568495647, 206158430447, 214748365067, 223338303719, 231928234787, 240518168603, 249108103547, 257698038539, 266287975727, 292057776239, 309237645803, 326417515547, 343597385507, 360777253763, 377957124803, 395136991499, 412316861267, 429496730879, 446676599987, 463856468987, 481036337207, 498216206387, 515396078039, 532575944723, 584115552323, 618475290887, 652835029643, 687194768879, 721554506879, 755914244627, 790273985219, 824633721383, 858993459587, 893353198763, 927712936643, 962072674643, 996432414899, 1030792152539, 1065151889507, 1168231105859, 1236950582039, 1305670059983, 1374389535587, 1443109012607, 1511828491883, 1580547965639, 1649267441747, 1717986918839, 1786706397767, 1855425872459, 1924145348627, 1992864827099, 2061584304323, 2130303780503, 2336462210183, 2473901164367, 2611340118887, 2748779070239, 2886218024939, 3023656976507, 3161095931639, 3298534883999, 3435973836983, 3573412791647, 3710851743923, 3848290698467, 3985729653707, 4123168604483, 4260607557707, 4672924419707, 4947802331663, 5222680234139, 5497558138979, 5772436047947, 6047313952943, 6322191860339, 6597069767699, 6871947674003, 7146825580703, 7421703488567, 7696581395627, 7971459304163, 8246337210659, 8521215117407, 9345848837267, 9895604651243, 10445360463947, 10995116279639, 11544872100683, 12094627906847, 12644383722779, 13194139536659, 13743895350023, 14293651161443, 14843406975659, 15393162789503, 15942918604343, 16492674420863, 17042430234443, 18691697672867, 19791209300867, 20890720927823, 21990232555703, 23089744183799, 24189255814847, 25288767440099, 26388279068903, 27487790694887, 28587302323787, 29686813951463, 30786325577867, 31885837205567, 32985348833687, 34084860462083, 37383395344739, 39582418600883, 41781441856823, 43980465111383, 46179488367203, 48378511622303, 50577534878987, 52776558134423, 54975581392583, 57174604644503, 59373627900407, 61572651156383, 63771674412287, 65970697666967, 68169720924167, 74766790688867, 79164837200927, 83562883712027, 87960930223163, 92358976733483, 96757023247427, 101155069756823, 105553116266999, 109951162779203, 114349209290003, 118747255800179, 123145302311783, 127543348823027, 131941395333479, 136339441846019, 149533581378263, 158329674402959, 167125767424739, 175921860444599, 184717953466703, 193514046490343, 202310139514283, 211106232536699, 219902325558107, 228698418578879, 237494511600287, 246290604623279, 255086697645023, 263882790666959, 272678883689987, 299067162755363, 316659348799919, 334251534845303, 351843720890723, 369435906934019, 387028092977819, 404620279022447, 422212465067447, 439804651111103, 457396837157483, 474989023199423, 492581209246163, 510173395291199, 527765581341227, 545357767379483, 598134325510343, 633318697599023, 668503069688723, 703687441776707, 738871813866287, 774056185954967, 809240558043419, 844424930134187, 879609302222207, 914793674313899, 949978046398607, 985162418489267, 1020346790579903, 1055531162666507, 1090715534754863};
    

