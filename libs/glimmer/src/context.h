//Copyright (c) 2003, The Institute for Genomic Research (TIGR), Rockville,
// Maryland, U.S.A.  All rights reserved.

//  A. L. Delcher
//
//  Written:  24 April 97
//
//     File:  ~delcher/TIGR/context.h
//  Version:  1.01  31 Jul 97
//
//  Routines to implement the nonuniform context Markov models described in
//  the Ristad/Thomas paper.


#ifndef  CONTEXT_H_INCLUDED
#define  CONTEXT_H_INCLUDED


#include  "delcher.h"
#include "strarray.h"

const int  SIMPLE_MODEL_LEN = 6;
const double  MIN_LOG_PROB_FACTOR = -6.0;




void  Simple_Evaluate  (char X [], int T, int Model_Len,
                        String_Array & Delta0, String_Array & Delta1,
                        String_Array & Delta2, double & Prob_X)

//  Set  Prob_X  to the log of the probability of generating DNA string
//  X [1 .. T]  in the simple nonhomogeneous Markov model with
//  model length  Model_Len  and conditional probabilites Delta .

  {
   int  t, Len;

   Prob_X = 0.0;
   for  (t = 1;  t <= T;  t ++)
     {
      Len = Min (t, Model_Len);
      switch  (t % 3)
        {
         case  0 :
           Prob_X += log (Delta0 (X + t - Len + 1, Len));
           break;
         case  1 :
           Prob_X += log (Delta1 (X + t - Len + 1, Len));
           break;
         case  2 :
           Prob_X += log (Delta2 (X + t - Len + 1, Len));
           break;
        }
     }

   Prob_X = Max (Prob_X, MIN_LOG_PROB_FACTOR * T);

   return;
  }



void  My_Fast_Evaluate  (char X [], int T, int Model_Len,
                        String_Array & Delta0, String_Array & Delta1,
                        String_Array & Delta2, double & Prob_X)

//  Set  Prob_X  to the log of the probability of generating DNA string
//  X [1 .. T]  in the simple nonhomogeneous Markov model with
//  model length  Model_Len  and conditional probabilites Delta .
//  Same as  Simple_Evaluate  but keeps track of subscripts for
//  delta values to go faster and assumes  Delta  entries have previously
//  been converted to logs.

  {
   int  t, Base, Len, Power, Sub;

   Prob_X = 0.0;

   Base = Sub = 0;
   Power = 1;
   Len = Min (T, Model_Len);
   for  (t = 1;  t <= Len;  t ++)
     {
      Base += Power;
      Sub = Sub * ALPHABET_SIZE + Delta0 . Value (X [t]);
      switch  (t % 3)
        {
         case  0 :
           Prob_X += exp(Delta0 [Base + Sub]);
           break;
         case  1 :
           Prob_X += exp(Delta1 [Base + Sub]);
           break;
         case  2 :
           Prob_X += exp(Delta2 [Base + Sub]);
           break;
        }
      if  (t < Len)
          Power *= ALPHABET_SIZE;
     }
   
   for  ( ;  t <= T;  t ++)
     {
      Sub = (Sub % Power) * ALPHABET_SIZE + Delta0 . Value (X [t]);
      switch  (t % 3)
        {
         case  0 :
           Prob_X += exp(Delta0 [Base + Sub]);
           break;
         case  1 :
           Prob_X += exp(Delta1 [Base + Sub]);
           break;
         case  2 :
           Prob_X += exp(Delta2 [Base + Sub]);
           break;
        }
     }

   //Prob_X = Max (Prob_X, MIN_LOG_PROB_FACTOR * T);
   Prob_X=Prob_X/T;

   return;
  }


/*void  Fast_Evaluate  (char X [], int T, int Model_Len,
  String_Array & Delta0, String_Array & Delta1,
  String_Array & Delta2, double & Prob_X)
  
  //  Set  Prob_X  to the log of the probability of generating DNA string
  //  X [1 .. T]  in the simple nonhomogeneous Markov model with
  //  model length  Model_Len  and conditional probabilites Delta .
  //  Same as  Simple_Evaluate  but keeps track of subscripts for
  //  delta values to go faster and assumes  Delta  entries have previously
  //  been converted to logs.
  
  {
  int  t, Base, Len, Power, Sub;

  Prob_X = 0.0;

  Base = Sub = 0;
  Power = 1;
  Len = Min (T, Model_Len);
  for  (t = 1;  t <= Len;  t ++)
  {
  Base += Power;
  Sub = Sub * ALPHABET_SIZE + Delta0 . Value (X [t]);
  switch  (t % 3)
  {
  case  0 :
  Prob_X += Delta0 [Base + Sub];
  break;
  case  1 :
  Prob_X += Delta1 [Base + Sub];
  break;
  case  2 :
  Prob_X += Delta2 [Base + Sub];
  break;
  }
  if  (t < Len)
  Power *= ALPHABET_SIZE;
  }
   
  for  ( ;  t <= T;  t ++)
  {
  Sub = (Sub % Power) * ALPHABET_SIZE + Delta0 . Value (X [t]);
  switch  (t % 3)
  {
  case  0 :
  Prob_X += Delta0 [Base + Sub];
  break;
  case  1 :
  Prob_X += Delta1 [Base + Sub];
  break;
  case  2 :
  Prob_X += Delta2 [Base + Sub];
  break;
  }
  }
  
  Prob_X = Max (Prob_X, MIN_LOG_PROB_FACTOR * T);
  
  return;
  }*/


void  Easy_Eval  (char X [], int T, String_Array & Lambda0,
                  String_Array & Lambda1, String_Array & Lambda2,
                  String_Array & Delta0, String_Array & Delta1,
                  String_Array & Delta2, double & Prob_X)

//  Set  Prob_X  to the log of the probability of generating DNA string
//  X [1 .. T]  in the nonuniform context model with parameters
//  Delta  and  Lambda .

  {
   int  t, Len;

   Prob_X = 0.0;
   for  (t = 1;  t <= T;  t ++)
     {
      Len = Min (t, SIMPLE_MODEL_LEN);
      switch  (t % 3)
        {
         case  0 :
           if  (Len < SIMPLE_MODEL_LEN || Lambda0 (X + t - Len + 1, Len - 1) > 0.999)
               Prob_X += log (Delta0 (X + t - Len + 1, Len));
             else
               Prob_X += log (Delta0 (X + t - SIMPLE_MODEL_LEN + 2, SIMPLE_MODEL_LEN - 1));
           break;
         case  1 :
           if  (Len < SIMPLE_MODEL_LEN || Lambda1 (X + t - Len + 1, Len - 1) > 0.999)
               Prob_X += log (Delta1 (X + t - Len + 1, Len));
             else
               Prob_X += log (Delta1 (X + t - SIMPLE_MODEL_LEN + 2, SIMPLE_MODEL_LEN - 1));
           break;
         case  2 :
           if  (Len < SIMPLE_MODEL_LEN || Lambda2 (X + t - Len + 1, Len - 1) > 0.999)
               Prob_X += log (Delta2 (X + t - Len + 1, Len));
             else
               Prob_X += log (Delta2 (X + t - SIMPLE_MODEL_LEN + 2, SIMPLE_MODEL_LEN - 1));
           break;
        }
     }

   return;
  }



void  Evaluate  (char X [], int T, String_Array & Lambda0,
                 String_Array & Lambda1, String_Array & Lambda2,
                 String_Array & Delta0, String_Array & Delta1,
                 String_Array & Delta2, double & Prob_X,int Model_Len)

//  Set  Prob_X  to the log of the probability of generating DNA string
//  X [1 .. T]  in the nonuniform context model with parameters
//  Delta  and  Lambda .

  {
   double  L, P, PC;
   int  i, t, Max_Context_Len;

   Prob_X = 0.0;
   for  (t = 0;  t < T;  t ++)
     {
//      Max_Context_Len = Min (t, SIMPLE_MODEL_LEN - 1);
      Max_Context_Len = Min (t, Model_Len - 1);
      P = 0.0;
      PC = 1.0;
      for  (i = Max_Context_Len;  i > 0 && PC > 0.0;  i --)
        {
         switch  ((t + 1) % 3)
           {
            case  0 :
              L = Lambda0 (X + t - i + 1, i);
              P += PC * L * Delta0 (X + t - i + 1, i + 1);
              PC *= 1 - L;
              break;
            case  1 :
              L = Lambda1 (X + t - i + 1, i);
              P += PC * L * Delta1 (X + t - i + 1, i + 1);
              PC *= 1 - L;
              break;
            case  2 :
              L = Lambda2 (X + t - i + 1, i);
              P += PC * L * Delta2 (X + t - i + 1, i + 1);
              PC *= 1 - L;
              break;
           }
        }
      switch  ((t + 1) % 3)
        {
         case  0 :
           P += PC * Delta0 (X + t - i + 1, i + 1);
           break;
         case  1 :
           P += PC * Delta1 (X + t - i + 1, i + 1);
           break;
         case  2 :
           P += PC * Delta2 (X + t - i + 1, i + 1);
           break;
        }

      Prob_X += log (P);
     }

   Prob_X = Max (Prob_X, MIN_LOG_PROB_FACTOR * T);

   return;
  }



void  Forward  (char X [], int T, double * * Alpha,
                double * * Transition, double & Prob_X,
                String_Array & Lambda0, String_Array & Lambda1,
                String_Array & Lambda2, String_Array & Delta0,
                String_Array & Delta1, String_Array & Delta2,
                double * A_Scale,int Model_Len)

//  Set values of  Alpha [t] [i] ,  0 <= t <= T ,
//  0 <= i < MODEL_LEN ,  to be the probability
//  of generating  X [1 .. t] , then choosing a context of length  i ,
//  and then generating the next symbol  X [t + 1] .
//  Set  Transition [t] [i]  to the probability of choosing a
//  context of length  i  and generating the next symbol, given
//  the current history  X [1 .. t] .  Set  Prob_X  to the log of
//  the probability
//  of generating the entire string  X [1 .. T] .  A_Scale [1 .. T]
//  is set to the log of the scaling factor for the probability
//  values.

  {
   double  A, L, PC;
   int  i, t, Max_Context_Len;
   
   A = 0.0;
   for  (i = 0;  i <= T;  i ++)
     A_Scale [i] = 0.0;

   for  (t = 0;  t < T;  t ++)
     {
      PC = 1.0;
      Max_Context_Len = Min (t, Model_Len - 1);

      for  (i = Max_Context_Len;  i > 0;  i --)
        {
         switch  ((t + 1) % 3)
           {
            case  0 :
              L = Lambda0 (X + t - i + 1, i);
              Transition [t] [i] = PC * L * Delta0 (X + t - i + 1, i + 1);
              PC *= 1 - L;
              break;
            case  1 :
              L = Lambda1 (X + t - i + 1, i);
              Transition [t] [i] = PC * L * Delta1 (X + t - i + 1, i + 1);
              PC *= 1 - L;
              break;
            case  2 :
              L = Lambda2 (X + t - i + 1, i);
              Transition [t] [i] = PC * L * Delta2 (X + t - i + 1, i + 1);
              PC *= 1 - L;
              break;
           }
         Alpha [t] [i] = Transition [t] [i];
         A += Alpha [t] [i];
        }

      switch  ((t + 1) % 3)
        {
         case  0 :
           Transition [t] [0] = PC * Delta0 (X + t + 1, 1);
           break;
         case  1 :
           Transition [t] [0] = PC * Delta1 (X + t + 1, 1);
           break;
         case  2 :
           Transition [t] [0] = PC * Delta2 (X + t + 1, 1);
           break;
        }
      Alpha [t] [0] = Transition [t] [0];
      A += Alpha [t] [0];

      A_Scale [t + 1] = A_Scale [t] + log (A);
      A = 1.0;
     }

   Prob_X = A_Scale [T];

   return;
  }



void  Backward  (char X [], int T, double Beta [],
                 double * * Transition, double * B_Scale,int Model_Len)

//  Set values of  Beta [t] ,  0 <= t <= T  to the probability of
//  generating the remaining symbols  X [t + 1 .. T] given that
//  the current history is  X [1 .. t] .  Uses the values  Transition
//  computed in  Forward () .

  {
   double  B;
   int  i, t, Max_Context_Len;

   Beta [T] = 1.0;
   B_Scale [T] = 0.0;

   for  (t = T - 1;  t >= 0;  t --)
     {
      Beta [t] = 0.0;
      Max_Context_Len = Min (t, Model_Len - 1);

      B_Scale [t] = log (Beta [t + 1]) + B_Scale [t + 1];
      B = Beta [t + 1] * exp (B_Scale [t + 1] - B_Scale [t]);

      for  (i = Max_Context_Len;  i >= 0;  i --)
        Beta [t] += Transition [t] [i] * B;
     }

   return;
  }



void  Calc_Gamma  (char X [], int T, double * * Gamma,
                   String_Array & Lambda0, String_Array & Lambda1,
                   String_Array & Lambda2, String_Array & Delta0,
                   String_Array & Delta1, String_Array & Delta2,
                   double & Prob_X,int Model_Len)

//  Calculate the transition probabilities  Gamma [t] [i]  of
//  the model for the string  X [1 .. T] .  Use the probability
//  parameters in  Lambda  and  Delta.  Set  Prob_X  to the
//  probability of this string being generated in the model.

  {
   double  Check, * * Alpha, * * Transition, * Beta;
   double  * A_Scale, * B_Scale;
   int  i, t, Max_Context_Len;

   Alpha = (double * *) Safe_malloc ((1 + T) * sizeof (double *));
   Transition = (double * *) Safe_malloc ((1 + T) * sizeof (double *));
   for  (t = 0;  t <= T;  t ++)
     {
      Alpha [t] = (double *) Safe_malloc (Model_Len * sizeof (double));
      Transition [t] = (double *) Safe_malloc (Model_Len * sizeof (double));
     }
   Beta = (double *) Safe_malloc ((1 + T) * sizeof (double));
   A_Scale = (double *) Safe_malloc ((1 + T) * sizeof (double));
   B_Scale = (double *) Safe_malloc ((1 + T) * sizeof (double));

   Forward (X, T, Alpha, Transition, Prob_X, Lambda0, Lambda1, Lambda2,
                  Delta0, Delta1, Delta2, A_Scale,Model_Len);
   Backward (X, T, Beta, Transition, B_Scale,Model_Len);

   Check = 0.0;
   for  (t = 0;  t < T;  t ++)
     {
      Max_Context_Len = Min (t, Model_Len - 1);

      for  (i = Max_Context_Len;  i >= 0;  i --)
        {
         Gamma [t] [i] = Alpha [t] [i] * Beta [t + 1]
                               * exp (A_Scale [t] + B_Scale [t + 1] -  Prob_X);
         Check += Gamma [t] [i];
        }
     }

//   printf ("Check = %f  T = %d\n", Check, T);         // Should = T

   for  (t = 0;  t <= T;  t ++)
     {
      free (Alpha [t]);
      free (Transition [t]);
     }
   free (Alpha);
   free (Transition);
   free (Beta);
   free (A_Scale);
   free (B_Scale);

   return;
  }



void  Expectation_Step  (char X [], int T, double * * Gamma,
                         String_Array & L_Plus0, String_Array & L_Plus1,
                         String_Array & L_Plus2, String_Array & L_Minus0,
                         String_Array & L_Minus1, String_Array & L_Minus2,int Model_Len)

//  Set the values of  L_Plus  and  L_Minus  according to the transitions
//  in  Gamma  for the string  X [1 .. T] .

  {
   int  i, k, t, Max_Context_Len;

   for  (t = T - 1;  t >= 0;  t --)
     {
      Max_Context_Len = Min (t, Model_Len - 1);

      for  (i = Max_Context_Len;  i > 0;  i --)
        {
         switch  ((t + 1) % 3)
           {
            case  0 :
              L_Plus0 . Incr (X + t - i + 1, i, Gamma [t] [i]);
              break;
            case  1 :
              L_Plus1 . Incr (X + t - i + 1, i, Gamma [t] [i]);
              break;
            case  2 :
              L_Plus2 . Incr (X + t - i + 1, i, Gamma [t] [i]);
              break;
           }
         for  (k = i + 1;  k <= Max_Context_Len;  k ++)
           switch  ((t + 1) % 3)
             {
              case  0 :
                L_Minus0 . Incr (X + t - k + 1, k, Gamma [t] [i]);
                break;
              case  1 :
                L_Minus1 . Incr (X + t - k + 1, k, Gamma [t] [i]);
                break;
              case  2 :
                L_Minus2 . Incr (X + t - k + 1, k, Gamma [t] [i]);
                break;
             }
        }
     }

   return;
  }



#endif

