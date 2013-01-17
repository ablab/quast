//Copyright (c) 2003 by  Mihaela Pertea and A. L. Delcher.

//   A. L. Delcher
//
//     File:  ~delcher/TIGR/nonuniform.h
//  Version:  1.02  4 Dec 97
//
//  Definitions for an array class whose index is strings.
//


#ifndef  STRARRAY_H_INCLUDED
#define  STRARRAY_H_INCLUDED


#include  "delcher.h"
#include  "gene.h"


#define  USE_CHI_SQUARE  1
#define  NEW_VERSION  0

const double  ESTIMATE_THRESHOLD = 50.0;
const double  MINIMUM_DELTA_VAL = 1e-5;
const double  MINIMUM_COUNT = 0.5;
const double  MIN_CORRECT_PREDICTS = 15.0;
const int  SAMPLE_SIZE_BOUND = 400;
const double  SAMPLE_WEIGHT = 400.0;
const double  SIGNIFICANCE_THRESHOLD = 0.01;
const double  Z_SCORE_THRESHOLD = 2.33;
const double  CHI2_THRESHOLD = 11.3;    // 0.01 significance  v=3


float  Analyze_Counts  (float [], float [], int);
float  New_Analyze  (float [], float [], int, float []);
int  Is_Significant  (float, double, double);
void  Reverse_Complement  (char [], long int);


class  String_Array
  {
  private:
    float  * Val;
    int  Max_Str_Len;
    int  Alphabet_Size;
    int  Num_Entries;

    double  Combine  (double, double);
    int  String_To_Sub  (char [], int);

  public:
    String_Array  ();
    String_Array  (int, int);
    ~ String_Array  ();
    void  Append  (char []);
    void  Combine  (String_Array &, String_Array &);
    void  Condense  (String_Array &);
    void  Convert_To_Logs  ();
    float *  Distribution  (char [], int);
    void  Estimate  (String_Array &);
    void  Incr  (char [], int, double);
    double  Maximization_Step  (String_Array &, String_Array &);
    double  Min_Val  ();
    void  Normalize  ();
    void  Read  (char []);
    void  Read  (FILE *);
    void  Set  (double = 0.0);
    void  Set_All  (double = 0.0);
    void  Set_Lambda  (String_Array &);
    void  Set_Single  (String_Array * [], String_Array * []);
    int  Value  (char);
    void  Write  (char []);
    double  operator []  (int i)
      {
       return  Val [i];
      }
    float &  operator ()  (char [], int);
  };



String_Array :: String_Array  ()

//  Default constructor.

  {
   Val = NULL;
   Num_Entries = Max_Str_Len = Alphabet_Size = 0;
  }



String_Array :: String_Array  (int L, int S)

//  Construct a  String_Array  that can be indexed by strings of length
//  up to  L  over an alphabet of size  S .

  {
   int  i;

   assert (L > 0 && S > 0);

   Max_Str_Len = L;
   Alphabet_Size = S;

   Num_Entries = int (pow (S, L + 1) - 1) / (S - 1);
   Val = (float *) Safe_malloc (Num_Entries * sizeof (float));

   Val [0] = 1.0;
   for  (i = 1;  i < Num_Entries;  i ++)
     Val [i] = 0.0;
  }



String_Array :: ~ String_Array  ()

//  Destroy this  String_Array  by freeing its memory.

  {
   free (Val);
  }



void  String_Array :: Append  (char S [])

//  Append the values of this  String_Array  to the file named  S .

  {
   FILE  * fp;
   int  i;

   fp = fopen (S, "a");
   if  (fp == NULL)
       {
        fprintf (stderr, "ERROR:  Could not open file \"%s\"\n", S);
        exit (-1);
       }

   fprintf (fp, "%d %d %d\n", Max_Str_Len, Alphabet_Size, Num_Entries);
   for  (i = 0;  i < Num_Entries;  i ++)
     fprintf (fp, "%6.5f\n", Val [i]);

   fclose (fp);

   return;
  }



double  String_Array :: Combine  (double Plus, double Minus)

//  Set the value  Plus / (Plus + Minus) .

  {
   double  Denom;

   Denom = Plus + Minus;
   if  (Denom == 0)
       return  0.0;
     else
       return  Plus / Denom;
  }



void  String_Array :: Combine  (String_Array & Plus,
                                String_Array & Minus)

//  Set all the values in this  String_Array  to corresponding
//  value  Plus / (Plus + Minus) .

  {
   int  i;

   for  (i = 0;  i < Num_Entries;  i ++)
     Val [i] = Combine (Plus . Val [i], Minus . Val [i]);

   return;
  }



void  String_Array :: Condense  (String_Array & L)

//  Reset the delta values for  this  string array to be
//  used as a simple model that is equivalent to the context model
//  using the lambda values in  L .

  {
   long int  i, Ct, Prev_i, Prev_i_Start, Prev_Size;
   long int  Size, Sub;

   assert (Max_Str_Len == L . Max_Str_Len + 1);

   Val [0] = 1.0;

   Ct = Prev_i = Prev_i_Start = Sub = 0;
   Prev_Size = 1;
   Size = Alphabet_Size;

   for  (i = 1;  i < Num_Entries;  i ++)
     {
      Val [i] = L . Val [Sub] * Val [i]
                     + (1.0 - L . Val [Sub]) * Val [Prev_i_Start + Prev_i];

      if  (++ Ct == Size)
          {
           Ct = 0;
           Prev_i_Start += Prev_Size;
           Prev_Size  = Size;
           Size *= Alphabet_Size;
           Prev_i = 0;
          }
        else
          Prev_i = (Prev_i + 1) % Prev_Size;

      if  (i % Alphabet_Size == 0)
          Sub ++;
     }

   return;
  }



void  String_Array :: Convert_To_Logs  ()

//  Reset each values to its logarithm.

  {
   int  i;

   for  (i = 0;  i < Num_Entries;  i ++)
     Val [i] = log (Val [i]);

   return;
  }



float *  String_Array :: Distribution  (char S [], int L)

//  Return a pointer to the first element in the array representing the
//  probability distribution of the next character occurring after
//  the context  S [0 .. L-1] .

  {
   char  * Tmp;
   int  i;

   assert (L >= 0);
   Tmp = (char *) Safe_malloc (1 + L);
   strncpy (Tmp, S, L);
   Tmp [L] = 'a';

   i = String_To_Sub (Tmp, L + 1);
   free (Tmp);

   return  Val + i;
  }



void  String_Array :: Estimate  (String_Array & D)

//  Estimate lambda values for  this  string array from
//  delta counts in  D .

  {
   int  i;

   assert (Max_Str_Len == D . Max_Str_Len - 1);

   Val [0] = 1.0;
   for  (i = 1;  i < Num_Entries;  i ++)
     Val [i] = Min (0.99, D . Val [i] / ESTIMATE_THRESHOLD);

   return;
  }



void  String_Array :: Incr  (char S [], int L, double D)

//  Add  D  to the entry at the subscript indicated by string
//  S [0 .. L-1]  in this  String_Array .

  {
   int  i;

   i = String_To_Sub (S, L);
   Val [i] += D;

   return;
  }


double  String_Array :: Maximization_Step  (String_Array & Plus,
                                          String_Array & Minus)

//  Reset the values in this  String_Array  using  Plus  and  Minus .
//  Return the maximum change between an old and new value.

  {
   double  Denom, Diff, Old, Max_Change = 0.0;
   int  i;

   Val [0] = 1.0;
   for  (i = 1;  i < Num_Entries;  i ++)
     {
      Denom = Plus . Val [i] + Minus . Val [i];
      Old = Val [i];
      if  (Denom != 0)
          {
           Val [i] = Plus . Val [i] / Denom;
           if  ((Diff = fabs (Val [i] - Old)) > Max_Change)
               Max_Change = Diff;
          }
     }

   return  Max_Change;
  }



double  String_Array :: Min_Val  ()

//  Return the minimum value in this  String_Array .

  {
   double  Min;
   int  i;

   Min = Val [0];

   for  (i = 1;  i < Num_Entries;  i ++)
     {
      if  (Val [i] < Min)
          Min = Val [i];
     }

   return  Min;
  }



void  String_Array :: Normalize  ()

//  Convert the values in this  String_Array  to probabilities.

  {
   double  Sum;
   long int  j, Ct, Prev_Size, Prev_Start, Prev_Sub, Size, Start;

   Val [0] = 1.0;

   Ct = 0;
   Prev_Size = 1;
   Prev_Start = 0;
   Prev_Sub = 0;
   Size = Alphabet_Size;

   for  (Start = 1;  Start < Num_Entries;  Start += Alphabet_Size)
     {
      if  (Prev_Size > 1)
          for  (j = 0;  j < Alphabet_Size;  j ++)
            Val [Start + j] += Val [Prev_Start + Prev_Sub + j];

      Sum = 0.0;
      for  (j = 0;  j < Alphabet_Size;  j ++)
        Sum += Val [Start + j];
      if  (Sum == 0.0)
          {
           for  (j = 0;  j < Alphabet_Size;  j ++)
             Val [Start + j] = 0.0;
          }
        else
          {
           for  (j = 0;  j < Alphabet_Size;  j ++)
             {
              Val [Start + j] /= Sum;
              if  (Val [Start + j] < MINIMUM_DELTA_VAL)
                  Val [Start + j] = MINIMUM_DELTA_VAL;
             }
          }

      if  (++ Ct == Size)
          {
           Ct = 0;
           Prev_Start += Prev_Size;
           Prev_Size = Size;
           Size *= Alphabet_Size;
           Prev_Sub = 0;
          }
        else
          Prev_Sub = (Prev_Sub + Alphabet_Size) % Prev_Size;
     }

   return;
  }



void  String_Array :: Read  (char S [])

//  Read the values of this  String_Array  from the file named  S .

  {
   FILE  * fp;
   int  i, M, A, N;

   fp = fopen (S, "r");
   if  (fp == NULL)
       {
        fprintf (stderr, "ERROR:  Could not open file \"%s\"\n", S);
        exit (-1);
       }

   fscanf (fp, "%d %d %d\n", & M, & A, & N);
   assert (M == Max_Str_Len && A == Alphabet_Size && N == Num_Entries);
   for  (i = 0;  i < Num_Entries;  i ++)
     fscanf (fp, "%e\n", Val + i);
   Val [0] = 1.0;

   fclose (fp);

   return;
  }



void  String_Array :: Read  (FILE * fp)

//  Read the values of this  String_Array  from the file  fp ,
//  which should already be open.  Leave it open so that the next
//  String_Array  can be read from the same file.

  {
   int  i, M, A, N;

   fscanf (fp, "%d %d %d\n", & M, & A, & N);
   assert (M == Max_Str_Len && A == Alphabet_Size && N == Num_Entries);
   for  (i = 0;  i < Num_Entries;  i ++)
     fscanf (fp, "%e\n", Val + i);
   Val [0] = 1.0;

   return;
  }



void  String_Array :: Set  (double X)

//  Set the value of all elements in this  String_Array  to  X .

  {
   int  i;

   Val [0] = 1.0;
   for  (i = 1;  i < Num_Entries;  i ++)
     Val [i] = X;

   return;
  }



void  String_Array :: Set_All  (double X)

//  Set *all* the value of all elements in this  String_Array  to  X .

  {
   int  i;

   for  (i = 0;  i < Num_Entries;  i ++)
     Val [i] = X;

   return;
  }



void  String_Array :: Set_Lambda  (String_Array & D)

//  Estimate lambda values for  this  string array from
//  delta counts in  D .

  {
   float  * Prob, Sum;
   int  Len;
   long int  Prev_Start, Prev_Sub, Size, Sub;
   long int  i, Ct, Prev_i, Prev_i_Start, Prev_Size, Used;

   assert (Max_Str_Len == D . Max_Str_Len - 1);

   printf ("Context Length Usage\n");
   printf (" Len      Used\n");
   Len = 1;
   Used = 0;
   Val [0] = 1.0;

   Prob = (float *) Safe_malloc (D . Num_Entries * sizeof (float));
   Sum = 0.0;
   for  (i = 1;  i <= Alphabet_Size;  i ++)
     Sum += D . Val [i];
   for  (i = 1;  i <= Alphabet_Size;  i ++)
     Prob [i] = D . Val [i] / Sum;

//  Process the counts in blocks based on context length (i.e.,
//  first block is contexts of length 1, second is contexts of length 2, ...)
//  Prev_Start  is the first subscript of the one-smaller context block
//  e.g.  i  represents the context string whose lambda value is being set.
//        Suppose it's  "at".  Then  Sub  will represent
//        string  "ata".  The counts at  Sub .. Sub + 3  will determine
//        the lambda value for string  i .  Prev_i  represents the next
//        smaller context, in this case "t", whose probabilities are already
//        computed and stored starting at  Prev_Sub .. Prev_Sub + 3
//        (representing strings "ta", "tc", "tg" and "tt").

   Sub = Alphabet_Size + 1;
   Prev_Start = 1;
   Prev_i_Start = 0;
   Prev_i = Prev_Sub = 0;
   Ct = 0;
   Prev_Size = 1;
   Size = Alphabet_Size;
   for  (i = 1;  i < Num_Entries;  i ++)
     {
#if  0
      if  (Val [Prev_i_Start + Prev_i] < 1.0)
          Val [i] = 0.0;
        else
          Val [i] = Analyze_Counts (D . Val + Sub,
                               D . Val + Prev_Start + Prev_Sub, Alphabet_Size);
#endif

//  Set the lambda value for this context based on the counts in  D . Val
//  and the probabilities in  Prob .
      Val [i] = New_Analyze (D . Val + Sub, Prob + Prev_Start + Prev_Sub,
                             Alphabet_Size, Prob + Sub);
#if  NEW_VERSION
      Val [i] *= Val [Prev_i_Start + Prev_i];
#endif
      if  (Val [i] > 0.0)
          Used ++;

      if  (++ Ct == Size)      //  If jump up to the next context length
          {
           printf ("%3d %9ld (%3d%%)\n", Len ++, Used,
                           int (0.5 + (100.0 * Used) / Size));
           Used = 0;
           Ct = 0;
           Prev_i_Start += Prev_Size;
           Prev_Start += Size;
           Prev_Size *= Alphabet_Size;
           Size *= Alphabet_Size;
           Prev_i = 0;
           Prev_Sub = 0;
          }
        else
          {
           Prev_Sub = (Prev_Sub + Alphabet_Size) % Size;
           Prev_i = (Prev_i + 1) % Prev_Size;
          }

      Sub += Alphabet_Size;
     }

   free (Prob);

   return;
  }



void  String_Array :: Set_Single  (String_Array * L_Plus [],
                                   String_Array * L_Minus [])

//  Set values so that for each string position of the maximum of all suffix values
//  is set to  1.0 .

  {
   double   Max, X;
   int  i, k, Ct, Len, Sub, Size;

   Ct = Len = 0;
   Size = 1;
   for  (i = 0;  i < Num_Entries;  i ++)
     {
      Max = Combine ((* L_Plus [Len]) [i], (* L_Minus [Len]) [i]);
      Sub = Len;
      for  (k = Len - 1;  k >= 0;  k --)
        {
         X = Combine ((* L_Plus [k]) [i], (* L_Minus [k]) [i]);
         if  (X > Max)
             {
              Max = X;
              Sub = k;
             }
        }
      if  (Max == 0.0 || Sub < Len || (* L_Plus [Len]) [i] < MIN_CORRECT_PREDICTS)
          Val [i] = 0.0;
        else
          Val [i] = 1.0;

      if  (++ Ct == Size)
          {
           Ct = 0;
           Len ++;
           Size *= Alphabet_Size;
          }
     }

   return;
  }



int  String_Array :: String_To_Sub  (char S [], int L)

//  Convert string  S [0 .. L-1]  to a subscript in the  Val  array.

  {
   int  i, Sub;

   assert (L <= Max_Str_Len);

   if  (L == 0)
       return  0;

   Sub = 0;
   for  (i = 0;  i < L;  i ++)
     {
      Sub = Sub * Alphabet_Size + Value (S [i]);
     }

   Sub += int (pow (Alphabet_Size, L) - 1) / (Alphabet_Size - 1);

   return  Sub;
  }



int  String_Array :: Value  (char Ch)

//  Return the numeric value of character  Ch .

  {
   switch  (tolower (Ch))
     {
      case  'a' :
        return  0;
      case  'c' :
        return  1;
      case  'g' :
        return  2;
      case  't' :
        return  3;
      default :
        fprintf (stderr, "ERROR:  Unexpected character \'%c\' (ASCII %d)\n",
                   Ch, int (Ch));
        exit (-1);
     }
   return(-1);
  }



void  String_Array :: Write  (char S [])

//  Write the values of this  String_Array  to the file named  S .

  {
   FILE  * fp;
   int  i;

   fp = fopen (S, "w");
   if  (fp == NULL)
       {
        fprintf (stderr, "ERROR:  Could not open file \"%s\"\n", S);
        exit (-1);
       }

   fprintf (fp, "%d %d %d\n", Max_Str_Len, Alphabet_Size, Num_Entries);
   for  (i = 0;  i < Num_Entries;  i ++)
     fprintf (fp, "%6.5f\n", Val [i]);

   fclose (fp);

   return;
  }



float &  String_Array :: operator ()  (char S [], int L)

//  Return a reference to the value associated with string  S [0 .. L-1] .

  {
   int  i;

   i = String_To_Sub (S, L);
   return  Val [i];
  }



float  Analyze_Counts  (float A [], float B [], int N)

//  Return the lambda value for the frequency counts in  A [0 .. N-1]
//  corresponding to the longer context string compared to the frequency
//  counts in  B [0 .. N-1]  for the next shorter suffix context string.

  {
   double  A_Sum, B_Sum, E;
   double  Chi2;
   int  i;

   A_Sum = 0.0;
   for  (i = 0;  i < N;  i ++)
     A_Sum += A [i];
   if  (A_Sum >= SAMPLE_SIZE_BOUND)
       return  1.0;

   B_Sum = 0.0;
   for  (i = 0;  i < N;  i ++)
     B_Sum += B [i];
   if  (B_Sum == 0.0)
       return  0.0;

#if  USE_CHI_SQUARE
   Chi2 = 0.0;
   for  (i = 0;  i < N;  i ++)
     {
      E = A_Sum * (B [i] + MINIMUM_COUNT) / (B_Sum + 4 * MINIMUM_COUNT);
      Chi2 += pow (A [i] - E, 2.0) / E;
     }
   if  (Chi2 >= CHI2_THRESHOLD)
       return  Min (1.0, A_Sum / SAMPLE_WEIGHT);
#else
   for  (i = 0;  i < N;  i ++)
     if  (Is_Significant (A [i], A_Sum, B [i] / B_Sum))
         return  Min (1.0, A_Sum / SAMPLE_WEIGHT);
#endif

   return  0.0;
  }



float  New_Analyze  (float A [], float P [], int N, float C [])

//  Return the lambda value for the frequency counts in  A [0 .. N-1]
//  corresponding to the longer context string compared to the predicted
//  probabilities in  P [0 .. N-1]  for the next shorter suffix context string.
//  Also set  C [0 .. N-1]  to the probabilities for this longer context
//  string.

  {
   const int  CHI2_ENTRIES = 7;
   float  Chi2_Val [CHI2_ENTRIES] = {2.37, 4.11, 6.25, 7.81, 9.35, 11.3, 12.8};
   float  Significance [CHI2_ENTRIES]
               = {0.50, 0.75, 0.90, 0.95, 0.975, 0.99, 0.995};
   double  A_Sum, Chi2, E, Lambda;
   int  i;

   A_Sum = 0.0;
   for  (i = 0;  i < N;  i ++)
     A_Sum += A [i];

//  If context string has occurred enough, then automatically use it
//  without interpolating, i.e., set lambda to 1.0 .
   if  (A_Sum >= SAMPLE_SIZE_BOUND)
       {
        for  (i = 0;  i < N;  i ++)
          C [i] = (A [i] + P [i]) / (A_Sum + 1.0);   // Add P so that probabilites are not 0.0
        return  1.0;
       }

//  Calculate chi-square statistic
   Chi2 = 0.0;
   for  (i = 0;  i < N;  i ++)
     {
      E = A_Sum * P [i];
      if  (E != 0.0)
          Chi2 += pow (A [i] - E, 2.0) / E;
     }
//  Find the significance level for that statistic
   for  (i = 0;  i < CHI2_ENTRIES && Chi2_Val [i] < Chi2;  i ++)
     ;
   if  (i == 0)
       Lambda = 0.0;
   else if  (i == CHI2_ENTRIES)
       Lambda = 1.0;
     else             // interpolate Signifance in array
       Lambda = Significance [i - 1]
                   + ((Chi2 - Chi2_Val [i - 1]) / (Chi2_Val [i] - Chi2_Val [i - 1]))
                         * (Significance [i] - Significance [i - 1]);
//  Multiply significance by a factor determined by the number of samples
   Lambda *= A_Sum / SAMPLE_WEIGHT;
   if  (Lambda > 1.0)
       Lambda = 1.0;

//  Set probabilities for this context as  Lambda  times probs from counts
//  plus  (1 - Lambda) times probs of one-shorter context
   for  (i = 0;  i < N;  i ++)
     C [i] = Lambda * ((A [i] + P [i]) / (A_Sum + 1.0))
               + (1.0 - Lambda) * P [i];

   return  Lambda;
  }



int  Is_Significant  (float K, double N, double P)

//  Return  TRUE  iff getting  K  positives in  N  trials
//  is significant when the probability of positive is  P .

  {
   double  Coeff, P_Pow, Q, Q_Pow, Std_Dev, Sum, z;
   double  i;
   int  j;

   assert (N >= 0.0 && K >= 0.0 && K <= N && P >= 0.0 && P <= 1.0);

   if  (N == 0.0)
       return  FALSE;

   Q = 1.0 - P;
   if  (N * P >= 5.0 && N * Q >= 5.0)
       {                                // Use Normal approximation
        Std_Dev = sqrt (N * P * Q);
        z = fabs ((K - N * P) / Std_Dev);
        return  (z >= Z_SCORE_THRESHOLD);
       }
   if  (K >= N * P)
       {
        Coeff = 1.0;
        P_Pow = pow (P, N);
        Q_Pow = 1.0;
        Sum = 0.0;
        j = 1;
        for  (i = N;  i >= K;  i --)
          {
           Sum += Coeff * P_Pow * Q_Pow;
           Coeff *= ((double) i) / j ++;
           P_Pow /= P;
           Q_Pow *= Q;
          }
        return  (Sum <= SIGNIFICANCE_THRESHOLD);
       }
     else
       {
        Coeff = 1.0;
        P_Pow = 1.0;
        Q_Pow = pow (Q, N);
        Sum = 0.0;
        j = 1;
        for  (i = 0;  i <= K;  i ++)
          {
           Sum += Coeff * P_Pow * Q_Pow;
           Coeff *= (N - i) / j ++;
           P_Pow *= P;
           Q_Pow /= Q;
          }
        return  (Sum <= SIGNIFICANCE_THRESHOLD);
       }
  }



void  Reverse_Complement  (char S [], long int T)

//  Set  S [1 .. T]  to its DNA reverse complement.

  {
   char  Ch;
   long int  i, j;

   for  (i = 1, j = T;  i < j;  i ++, j --)
     {
      Ch = S [j];
      S [j] = Complement (S [i]);
      S [i] = Complement (Ch);
     }

   if  (i == j)
       S [i] = Complement (S [i]);
  }



#endif
