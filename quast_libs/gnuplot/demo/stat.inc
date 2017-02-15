#
# $Id: stat.inc,v 1.3 2006/06/14 03:24:09 sfeam Exp $
#
# Library of Statistical Functions version 3.0
#
# Copyright (c) 1991, 1992 Jos van der Woude, jvdwoude@hut.nl

# History:
#    -- --- 1992 Jos van der Woude:  1st version
#    06 Jun 2006 Dan Sebald:  Defined PDF/CDF for whole real line or integer
#                             set and range checked all other parameters.

#
# Shortcut for testing if a variable is an integer
#
isint(x)=(int(x)==x)

# Define useful constants
fourinvsqrtpi=4.0/sqrt(pi)
invpi=1.0/pi
invsqrt2pi=1.0/sqrt(2.0*pi)
log2=log(2.0)
sqrt2=sqrt(2.0)
sqrt2invpi=sqrt(2.0/pi)
twopi=2.0*pi

#
#define 1.0/Beta function
#
Binv(p,q)=exp(lgamma(p+q)-lgamma(p)-lgamma(q))

# NOTE:
#
# The stat functions are defined appropriately for the whole real line or
# set of integers (the first input variable).  There are restrictions on
# some of the parameters, and an undefined value will result if the input
# value falls outside the parameter's range.
#
# The discrete PDFs (and some parameters) must have integer (natural number)
# inputs, otherwise an undefined result is produced.  This means the user
# must appropriately supply a discrete input set, perhaps by some form of
# scaling before and after calling the stat desired function.  To plot the
# output of such a discrete data set passed through a stat function, the
# user can make use of plotting features such as "with steps", and so on.

#
#define Probability Density Functions (PDFs)
#

# Arcsin PDF
arcsin(x,r)=r<=0?1/0:abs(x)>r?0.0:invpi/sqrt(r*r-x*x)

# Beta PDF
beta(x,p,q)=p<=0||q<=0?1/0:x<0||x>1?0.0:Binv(p,q)*x**(p-1.0)*(1.0-x)**(q-1.0)

# Binomial PDF
binom(x,n,p)=p<0.0||p>1.0||n<0||!isint(n)?1/0:\
  !isint(x)?1/0:x<0||x>n?0.0:exp(lgamma(n+1)-lgamma(n-x+1)-lgamma(x+1)\
  +x*log(p)+(n-x)*log(1.0-p))

# Cauchy PDF
# a location parameter, b > 0 scale parameter
cauchy(x,a,b)=b<=0?1/0:b/(pi*(b*b+(x-a)**2))

# Chi-square PDF
chisq(x,k)=k<=0||!isint(k)?1/0:\
  x<=0?0.0:exp((0.5*k-1.0)*log(x)-0.5*x-lgamma(0.5*k)-k*0.5*log2)

# Erlang PDF
erlang(x,n,lambda)=n<=0||!isint(n)||lambda<=0?1/0:\
  x<0?0.0:x==0?(n==1?real(lambda):0.0):exp(n*log(lambda)+(n-1.0)*log(x)-lgamma(n)-lambda*x)

# Extreme (Gumbel extreme value) PDF
extreme(x,mu,alpha)=alpha<=0?1/0:alpha*(exp(-alpha*(x-mu)-exp(-alpha*(x-mu))))

# F PDF
f(x,d1,d2)=d1<=0||!isint(d1)||d2<=0||!isint(d2)?1/0:\
  Binv(0.5*d1,0.5*d2)*(real(d1)/d2)**(0.5*d1)*x**(0.5*d1-1.0)/(1.0+(real(d1)/d2)*x)**(0.5*(d1+d2))

# Gamma PDF
# rho > 0 shape parameter, lambda > 0 inverse scale parameter
gmm(x,rho,lambda)=rho<=0||lambda<=0?1/0:\
  x<0?0.0:x==0?(rho>1?0.0:rho==1?real(lambda):1/0):\
  exp(rho*log(lambda)+(rho-1.0)*log(x)-lgamma(rho)-lambda*x)

# Geometric PDF
# p probability of success, x number of failures before first success
geometric(x,p)=p<=0||p>1?1/0:\
  !isint(x)?1/0:x<0||p==1?(x==0?1.0:0.0):exp(log(p)+x*log(1.0-p))

# Half normal PDF
halfnormal(x,sigma)=sigma<=0?1/0:x<0?0.0:sqrt2invpi/sigma*exp(-0.5*(x/sigma)**2)

# Hypergeometric PDF
# N objects, C of one class (N-C of another), d drawn without
# replacement, x drawn of class C.
hypgeo(x,N,C,d)=N<0||!isint(N)||C<0||C>N||!isint(C)||d<0||d>N||!isint(d)?1/0:\
  !isint(x)?1/0:x>d||x>C||x<0||x<d-(N-C)?0.0:exp(lgamma(C+1)-lgamma(C-x+1)-lgamma(x+1)+\
  lgamma(N-C+1)-lgamma(d-x+1)-lgamma(N-C-d+x+1)+lgamma(N-d+1)+lgamma(d+1)-lgamma(N+1))

# Laplace PDF
laplace(x,mu,b)=b<=0?1/0:0.5/b*exp(-abs(x-mu)/b)

# Logistic PDF
logistic(x,a,lambda)=lambda<=0?1/0:lambda*exp(-lambda*(x-a))/(1.0+exp(-lambda*(x-a)))**2

# Lognormal PDF
lognormal(x,mu,sigma)=sigma<=0?1/0:\
  x<0?0.0:invsqrt2pi/sigma/x*exp(-0.5*((log(x)-mu)/sigma)**2)

# Maxwell PDF
# a sqrt(2) times standard deviation of individual component of normal triple
maxwell(x,a)=a<=0?1/0:x<0?0.0:fourinvsqrtpi*a**3*x*x*exp(-a*a*x*x)

# Negative binomial PDF
# p probability of success, r number of success to complete,
# x failures before success r
negbin(x,r,p)=r<=0||!isint(r)||p<=0||p>1?1/0:\
  !isint(x)?1/0:x<0?0.0:p==1?(x==0?1.0:0.0):exp(lgamma(r+x)-lgamma(r)-lgamma(x+1)+\
  r*log(p)+x*log(1.0-p))

# Negative exponential PDF
nexp(x,lambda)=lambda<=0?1/0:x<0?0.0:lambda*exp(-lambda*x)

# Normal PDF
normal(x,mu,sigma)=sigma<=0?1/0:invsqrt2pi/sigma*exp(-0.5*((x-mu)/sigma)**2)

# Pareto PDF
pareto(x,a,b)=a<=0||b<0||!isint(b)?1/0:x<a?0:real(b)/x*(real(a)/x)**b

# Poisson PDF
poisson(x,mu)=mu<=0?1/0:!isint(x)?1/0:x<0?0.0:exp(x*log(mu)-lgamma(x+1)-mu)

# Rayleigh PDF
rayleigh(x,lambda)=lambda<=0?1/0:x<0?0.0:lambda*2.0*x*exp(-lambda*x*x)

# Sine PDF
# f frequency, a length
sine(x,f,a)=a<=0?1/0:\
  x<0||x>=a?0.0:f==0?1.0/a:2.0/a*sin(f*pi*x/a)**2/(1-sin(twopi*f))

# t (Student's t) PDF
t(x,nu)=nu<0||!isint(nu)?1/0:\
  Binv(0.5*nu,0.5)/sqrt(nu)*(1+real(x*x)/nu)**(-0.5*(nu+1.0))

# Triangular PDF
triangular(x,m,g)=g<=0?1/0:x<m-g||x>=m+g?0.0:1.0/g-abs(x-m)/real(g*g)

# Uniform PDF
uniform(x,a,b)=x<(a<b?a:b)||x>=(a>b?a:b)?0.0:1.0/abs(b-a)

# Weibull PDF
weibull(x,a,lambda)=a<=0||lambda<=0?1/0:\
  x<0?0.0:x==0?(a>1?0.0:a==1?real(lambda):1/0):\
  exp(log(a)+a*log(lambda)+(a-1)*log(x)-(lambda*x)**a)

#
#define Cumulative Distribution Functions (CDFs)
#

# Arcsin CDF
carcsin(x,r)=r<=0?1/0:x<-r?0.0:x>r?1.0:0.5+invpi*asin(x/r)

# incomplete Beta CDF
cbeta(x,p,q)=ibeta(p,q,x)

# Binomial CDF
cbinom(x,n,p)=p<0.0||p>1.0||n<0||!isint(n)?1/0:\
  !isint(x)?1/0:x<0?0.0:x>=n?1.0:ibeta(n-x,x+1.0,1.0-p)

# Cauchy CDF
# a location parameter, b > 0 scale parameter
ccauchy(x,a,b)=b<=0?1/0:0.5+invpi*atan((x-a)/b)

# Chi-square CDF
cchisq(x,k)=k<=0||!isint(k)?1/0:x<0?0.0:igamma(0.5*k,0.5*x)

# Erlang CDF
cerlang(x,n,lambda)=n<=0||!isint(n)||lambda<=0?1/0:x<0?0.0:igamma(n,lambda*x)

# Extreme (Gumbel extreme value) CDF
cextreme(x,mu,alpha)=alpha<=0?1/0:exp(-exp(-alpha*(x-mu)))

# F CDF
cf(x,d1,d2)=d1<=0||!isint(d1)||d2<=0||!isint(d2)?1/0:1.0-ibeta(0.5*d2,0.5*d1,d2/(d2+d1*x))

# incomplete Gamma CDF
# rho > 0 shape parameter, lambda > 0 inverse scale parameter
cgmm(x,rho,lambda)=rho<=0||lambda<=0?1/0:x<0?0.0:igamma(rho,x*lambda)

# Geometric CDF
# p probability of success, x number of failures before first success
cgeometric(x,p)=p<=0||p>1?1/0:\
  !isint(x)?1/0:x<0||p==0?0.0:p==1?1.0:1.0-exp((x+1)*log(1.0-p))

# Half normal CDF
chalfnormal(x,sigma)=sigma<=0?1/0:x<0?0.0:erf(x/sigma/sqrt2)

# Hypergeometric CDF
# N objects, C of one class (N-C of another), d drawn without
# replacement, x drawn of class C.
chypgeo(x,N,C,d)=N<0||!isint(N)||C<0||C>N||!isint(C)||d<0||d>N||!isint(d)?1/0:\
  !isint(x)?1/0:x<0||x<d-(N-C)?0.0:x>d||x>C?1.0:hypgeo(x,N,C,d)+chypgeo(x-1,N,C,d)

# Laplace CDF
claplace(x,mu,b)=b<=0?1/0:(x<mu)?0.5*exp((x-mu)/b):1.0-0.5*exp(-(x-mu)/b)

# Logistic CDF
clogistic(x,a,lambda)=lambda<=0?1/0:1.0/(1+exp(-lambda*(x-a)))

# Lognormal CDF
clognormal(x,mu,sigma)=sigma<=0?1/0:x<=0?0.0:cnormal(log(x),mu,sigma)

# Maxwell CDF
# a sqrt(2) times standard deviation of individual component of normal triple
cmaxwell(x,a)=a<=0?1/0:x<0?0.0:igamma(1.5,a*a*x*x)

# Negative binomial CDF
# p probability of success, r number of success to complete,
# x failures before success r
cnegbin(x,r,p)=r<=0||!isint(r)||p<=0||p>1?1/0:\
  !isint(x)?1/0:x<0?0.0:ibeta(r,x+1,p)

# Negative exponential CDF
cnexp(x,lambda)=lambda<=0?1/0:x<0?0.0:1-exp(-lambda*x)

# Normal CDF
cnormal(x,mu,sigma)=sigma<=0?1/0:0.5+0.5*erf((x-mu)/sigma/sqrt2)

# Pareto CDF
cpareto(x,a,b)=a<=0||b<0||!isint(b)?1/0:x<a?0.0:1.0-(real(a)/x)**b

# Poisson CDF
cpoisson(x,mu)=mu<=0?1/0:!isint(x)?1/0:x<0?0.0:1-igamma(x+1.0,mu)

# Rayleigh CDF
crayleigh(x,lambda)=lambda<=0?1/0:x<0?0.0:1.0-exp(-lambda*x*x)

# Sine CDF
# f frequency, a length
csine(x,f,a)=a<=0?1/0:\
  x<0?0.0:x>a?1.0:f==0?real(x)/a:(real(x)/a-sin(f*twopi*x/a)/(f*twopi))/(1.0-sin(twopi*f)/(twopi*f))

# t (Student's t) CDF
ct(x,nu)=nu<0||!isint(nu)?1/0:0.5+0.5*sgn(x)*(1-ibeta(0.5*nu,0.5,nu/(nu+x*x)))

# Triangular PDF
ctriangular(x,m,g)=g<=0?1/0:\
  x<m-g?0.0:x>=m+g?1.0:0.5+real(x-m)/g-(x-m)*abs(x-m)/(2.0*g*g)

# Uniform CDF
cuniform(x,a,b)=x<(a<b?a:b)?0.0:x>=(a>b?a:b)?1.0:real(x-a)/(b-a)

# Weibull CDF
cweibull(x,a,lambda)=a<=0||lambda<=0?1/0:x<0?0.0:1.0-exp(-(lambda*x)**a)

