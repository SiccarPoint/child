/***************************************************************************\
**
**  erosion.cpp
**
**  Functions for equilibrium checking, sediment transport and bed erosion
**    (detachment) objects.
**  Equilibrium check objects:
**    tEquilibCheck
**  Transport objects:
**    tSedTransPwrLawn
**    tSedTransWilcock
**    tSedTransMineTailings //added 4/00 ng
**  Detachment objects:
**    tBedErodePwrLaw
**
**    Created 1/98 gt; add tEqChk 5/98 sl
**
**    Modifications:
**     - detach capacity functions modified to use spatially variable
**       (ie node-based, not "global") critical shear stress, in
**       conjunction w/ veg module. GT, Jan 2000
**     - added DensifyMesh function to add new nodes in areas of high
**       erosion/deposition flux. (gt, 2/2000)
**     - modified detachment and power-law transport formulas to use
**       kt in truly SI units, with a conversion factor from Q in m3/yr
**       to m3/s added in the constructor -- so kt is READ in SI units,
**       and multiplied by the unit conversion factor before being
**       used in the detachment and transport equations. (GT 6/01)
**     - added functions for new class tSedTransPwrLawMulti (GT 2/02)
**
**    Known bugs:
**     - ErodeDetachLim assumes 1 grain size. If multiple grain sizes
**       are specified in the input file and the detachment limited
**       option is used, a crash will result when tLNode::EroDep
**       attempts to access array indices above 1. TODO (GT 3/00)
**
**  $Id: erosion.cpp,v 1.88 2002-02-11 09:12:35 gtucker Exp $
\***************************************************************************/

#include <math.h>
#include <assert.h>
#include <iomanip.h>
#include <string>
#include "erosion.h"

/***************************************************************************\
**  FUNCTIONS FOR CLASS tEquilibCheck
**
**  Functions to find and report system mass change rate.
**
**  5/98 SL
\***************************************************************************/

/***************************************************************************\
**  Constructors: default (no args) and given mesh and timer as args
\***************************************************************************/
tEquilibCheck::tEquilibCheck()
        : massList()
{
   meshPtr = 0;
   timePtr = 0;
   longTime = longRate = shortRate = 0.0;
}

tEquilibCheck::tEquilibCheck( tMesh< tLNode > &meshRef, tRunTimer &timeRef )
        : massList()
{
   meshPtr = &meshRef;
   timePtr = &timeRef;
   longTime = longRate = shortRate = 0.0;
   FindIterChngRate();
}

tEquilibCheck::tEquilibCheck( tMesh< tLNode > &meshRef, tRunTimer &timeRef,
                              tInputFile &fileRef )
        : massList()
{
   meshPtr = &meshRef;
   timePtr = &timeRef;
   longRate = shortRate = 0.0;
   longTime = fileRef.ReadItem( longTime, "EQUITIME" );
   FindIterChngRate();
}

tEquilibCheck::~tEquilibCheck()
{
   meshPtr = 0;
   timePtr = 0;
}


/***************************************************************************\
**  'get' and 'set' functions for tEquilibCheck:
\***************************************************************************/
double tEquilibCheck::getLongTime() const {return longTime;}

void tEquilibCheck::setLongTime( double val )
{longTime = ( val > 0 ) ? val : 0.0;}

const tMesh< tLNode > *tEquilibCheck::getMeshPtr() const {return meshPtr;}

tMesh< tLNode > *tEquilibCheck::getMeshPtrNC() {return meshPtr;}

void tEquilibCheck::setMeshPtr( tMesh< tLNode > &Ref )
{meshPtr = ( &Ref > 0 ) ? &Ref : 0;}

void tEquilibCheck::setMeshPtr( tMesh< tLNode > *Ptr )
{meshPtr = ( Ptr > 0 ) ? Ptr : 0;}

const tRunTimer *tEquilibCheck::getTimePtr() const {return timePtr;}

tRunTimer *tEquilibCheck::getTimePtrNC() {return timePtr;}

void tEquilibCheck::setTimePtr( tRunTimer &Ref )
{timePtr = ( &Ref > 0 ) ? &Ref : 0;}

void tEquilibCheck::setTimePtr( tRunTimer *Ptr )
{timePtr = ( Ptr > 0 ) ? Ptr : 0;}

double tEquilibCheck::getLongRate() const {return longRate;}

double tEquilibCheck::getShortRate() const {return shortRate;}


/***************************************************************************\
**  tEquilibCheck::FindIterChngRate()
**
**  Find average rate of elevation change since the last time the object was
**  called, as short a time as one model iteration.
**  Finds new average elevation, new time, and calculates rate based on last
**  avg. elev. and time.
\***************************************************************************/
double tEquilibCheck::FindIterChngRate()
{
   assert( timePtr > 0 && meshPtr > 0 );
   tArray< double > tmp(2), last;
   tmp[0] = timePtr->getCurrentTime();
   tMeshListIter< tLNode > nI( meshPtr->getNodeList() );
   tListIter< tArray< double > > mI( massList );
   tLNode *cn;
   double mass = 0.0;
   double area = 0.0;
   for( cn = nI.FirstP(); nI.IsActive(); cn = nI.NextP() )
   {
      mass += cn->getZ() * cn->getVArea();
      area += cn->getVArea();
   }
   tmp[1] = mass / area;
   if( !(massList.isEmpty()) )
   {
      last = *(mI.LastP());
      double dt = (tmp[0] - last[0]);
      assert( dt > 0.0 );
      shortRate = (tmp[1] - last[1]) / dt;
   }
   else
   {
        //cout << "tEquilibCheck::FindIterChngRate(), Warning: empty massList\n";
      assert( tmp[0] > 0 );
      shortRate = tmp[1] / tmp[0];
   }
   massList.insertAtBack( tmp );
   return shortRate;
}


/***************************************************************************\
**  tEquilibCheck::FindLongTermChngRate()
**
**  Find average rate of elevation change over a set time, as short as one
**  model iteration.
**  Calls FindIterChngRate() to update massList, then searches massList for
**  time to provide rate over a time >= longTime.
\***************************************************************************/
double tEquilibCheck::FindLongTermChngRate()
{
   FindIterChngRate();
   tListIter< tArray< double > > mI( massList );
   tArray< double > last = *(mI.LastP());
   tArray< double > ca, na;
   double dt, targetTime = last[0] - longTime;
   if( longTime == 0.0 || mI.FirstP() == mI.LastP() ) longRate = shortRate;
   else
   {
      ca = *(mI.FirstP());
      na = *(mI.NextP());
      while( na[0] < targetTime && !(mI.AtEnd()) )
      {
         ca = na;
         na = *(mI.NextP());
      }
      dt = last[0] - ca[0];
      assert( dt > 0 );
      longRate = (last[1] - ca[1]) / dt;
   }
   return longRate;
}


/***************************************************************************\
**  tEquilibCheck::FindLongTermChngRate( double newtime )
**
**  set longTime = newtime and call FindLongTermChngRate()
\***************************************************************************/
double tEquilibCheck::FindLongTermChngRate( double newtime )
{
   setLongTime( newtime );
   return FindLongTermChngRate();
}

   

/***************************************************************************\
**  FUNCTIONS FOR CLASS tBedErodePwrLaw
\***************************************************************************/

/***************************************************************************\
**  tBedErodePwrLaw Constructor
**
**  Reads in coefficients and exponents for the power law. "mb" is
**  assumed to be the exponent on specific discharge (L2/T), i.e.,
**  Dc ~ q^mb. It is then converted to a total discharge exponent,
**  i.e., Dc ~ Q^mb, by multiplying by (1-ws), where ws is the
**  at-a-station width-discharge relation. Before converting, we
**  define the drainage area exponent, ma = mb*(ws-wb). This
**  essentially describes the difference between at-a-station and
**  downstream width variations with discharge.
**
**  Modifications:
**   - Erosion rate is now calculated from specific discharge, which is
**     explicitly computed from Q/W, W = channel width computed in calls
**     to tStreamNet::FindChanGeom and tStreamNet::FindHydrGeom. As a
**     result, ma is now obsolete and mb is no longer modified after being
**     read in from file. mb is still the specific-discharge exponent,
**     NOT the total-discharge exponent. (GT 2/01)
**   - kt now read in SI units, with conversion factor between discharge
**     in m3/yr and shear stress (sic) in SI units factored in here in
**     this constructor. The derivation is:
**   
**       Tau (SI) = kt(SI) * Uconv * (Q(m/yr)/W)^mb * S^nb
**     
**     where Tau is shear stress or power or whatever, depending on
**     dimensions, and Uconv = SPY^-mb where SPY = # secs in a year
**     (GT 6/01)
**
\***************************************************************************/
//constructor: reads and sets the parameters
tBedErodePwrLaw::tBedErodePwrLaw( tInputFile &infile )
{
  double secPerYear = 365.25*24*3600.0;  // # secs in one year

   kb = infile.ReadItem( kb, "KB" );
   kt = infile.ReadItem( kt, "KT" );
   mb = infile.ReadItem( mb, "MB" ); // Specific q exponent
   //wb = infile.ReadItem( wb, "HYDR_WID_EXP_DS" );
   //ws = infile.ReadItem( ws, "HYDR_WID_EXP_STN" );
   //ma = mb*(ws-wb);  // Drainage area exponent
   //mb = mb*(1-ws);   // Convert mb to total-discharge exponent
   nb = infile.ReadItem( nb, "NB" );
   pb = infile.ReadItem( pb, "PB" );
   taucd = infile.ReadItem( taucd, "TAUCD" );

   // Add unit conversion factor for kt -- this is required to convert
   // the quantity (Q/W)^mb from units of years to units of seconds.
   kt = kt * pow( secPerYear, -mb );
}


/***************************************************************************\
**  tBedErode::DetachCapacity (1 of 3)
**
**  Computes the depth of erosion over a time interval dt assuming the
**  erosion rate = kb Q^mb S^nb
**
**  Input: n -- node at which to compute detachment capacity
**         dt -- time interval
**  Returns: the detachment depth
**  Assumptions: n->getSlope() does not return a negative value (returns neg.
**               only if infinite loop in getSlope()); kb, mb,
**               and nb all >=0.
**  Modifications:
**   - replaced uniform erodibility coefficient kb with erodibility of
**     topmost layer at node n (GT 8/98)
**   - added threshold term and third exponent pb (GT 4/99)
**   - added shear coefficient kt, width term, and moved erodibility coeff 
**     (GT 5/99) (to be ver 2.0.2)
**   - replaced spatially uniform taucd parameter with node variable
**     tauc (GT 1/00)
**   - placed channel width explicitly in the denominator rather than
**     have it be buried in exponents and coefficients, so as to be
**     consistent with the transport equations.
**     (GT 2/01)
\***************************************************************************/
double tBedErodePwrLaw::DetachCapacity( tLNode * n, double dt )
{
   double slp = n->getSlope(),
       tau, tauex;

   if( n->getFloodStatus() ) return 0.0;
   if( slp < 0.0 )
       ReportFatalError("neg. slope in tBedErodePwrLaw::DetachCapacity(tLNode*,double)");
   //tau =  ktb*pow( n->getQ(), mb )*pow( n->getDrArea(), ma ) * pow( slp, nb );
   tau =  kt*pow( n->getQ() / n->getHydrWidth(), mb ) * pow( slp, nb );
   n->setTau( tau );
   tauex = tau - n->getTauCrit();
   //cout << "tauex: " << tauex << endl;
   tauex = (tauex>0.0) ? tauex : 0.0;
   return( n->getLayerErody(0)*pow(tauex,pb)*dt );
}


/***************************************************************************\
**  tBedErode::DetachCapacity (2 of 3)
**
**  Computes the rate of erosion  = kb Q^mb S^nb
**
**  Input: n -- node at which to compute detachment capacity
**
**  Returns: the detachment rate
**  Assumptions: n->getSlope() does not return a negative value (returns neg.
**               only if infinite loop in getSlope()); kb, mb,
**               and nb all >=0.
**  Modifications:
**   - replaced uniform erodibility coefficient kb with erodibility of
**     topmost layer at node n (GT 8/98)
**   - added threshold term and third exponent pb (GT 4/99)
**   - added shear coefficient kt, width term, and moved erodibility coeff 
**     (GT 5/99) (ver 2.0.2)
**   - replaced spatially uniform taucd parameter with node variable
**     tauc (GT 1/00)
**   - placed channel width explicitly in the denominator rather than
**     have it be buried in exponents and coefficients, so as to be
**     consistent with the transport equations.
**     (GT 2/01)
\***************************************************************************/
double tBedErodePwrLaw::DetachCapacity( tLNode * n )
{
   //cout<<"in detach capacity "<<endl<<flush;
   assert( n->getQ()>=0.0 );
   assert( n->getQ()>=0.0 );
   
   if( n->getFloodStatus() ) return 0.0;
   double slp = n->getSlope();
   if( slp < 0.0 )
     ReportFatalError("neg. slope in tBedErodePwrLaw::DetachCapacity(tLNode*)");
   //double tau = ktb*pow( n->getQ(), mb )*pow( n->getDrArea(), ma )
   //    *pow( slp, nb );
   double tau = kt*pow( n->getQ() / n->getHydrWidth(), mb )*pow( slp, nb );
   if( n->getQ()<0.0 || n->getDrArea()<0.0 ) n->TellAll();
   assert( n->getQ()>=0.0 );
   assert( n->getDrArea()>=0.0 );
   n->setTau( tau );
   double erorate = tau - n->getTauCrit();
   //cout << "tau " << tau;
   //cout << " tauc " << n->getTauCrit() << endl;
   erorate = (erorate>0.0) ? erorate : 0.0;
   erorate = n->getLayerErody(0)*pow( erorate, pb );
   //if( n->getDrArea()>1e7 ) cout << " erorate: " << erorate << endl;
   n->setDrDt( -erorate );
   //cout << "2erorate: " << erorate << endl;
   return erorate;
}


/***************************************************************************\
**  tBedErode::DetachCapacity (3 of 3)
**
**  Computes the rate of erosion  = e* Q^mb S^nb
**  Here erodibility of layer is used as the coefficient for detach capacity
**
**  TODO: have this just call the other DetachCapacity and multiply by dt!
**  Also: consolidate w/ 2 of 3 by using default 0 parameter for layer #.
**
**  Input: n -- node at which to compute detachment capacity
**         i -- layer which you are computing detachment of
** 
**  Returns: the detachment rate
**  Assumptions: n->getSlope() does not return a negative value (returns neg.
**               only if infinite loop in getSlope()); kb, mb,
**               and nb all >=0.
**  Modifications:
**   - added shear coefficient kt, width term, and moved erodibility coeff 
**     (GT 5/99) (ver 2.0.2)
**   - replaced spatially uniform taucd parameter with node variable
**     tauc (GT 1/00)
**   - placed channel width explicitly in the denominator rather than
**     have it be buried in exponents and coefficients, so as to be
**     consistent with the transport equations.
**     (GT 2/01)
\***************************************************************************/
double tBedErodePwrLaw::DetachCapacity( tLNode * n, int i )
{
   if( n->getFloodStatus() ) return 0.0;
   double slp = n->getSlope();
   if( slp < 0.0 )
       ReportFatalError("neg. slope in tBedErodePwrLaw::DetachCapacity(tLNode*)");
   //double tau = ktb*pow( n->getQ(), mb )*pow( n->getDrArea(), ma )
   //    *pow( slp, nb );
   double tau = kt*pow( n->getQ() / n->getHydrWidth(), mb )*pow( slp, nb );
   n->setTau( tau );
   double erorate = tau - n->getTauCrit();
   //cout << "erorate: " << erorate << endl;
   erorate = (erorate>0.0) ? erorate : 0.0;
   erorate = n->getLayerErody(i)*pow( erorate, pb );
   n->setDrDt( -erorate );
//    if(n->getID() == 11 ){
//       cout<<"node 11 is in detach capacity"<<endl;
//       cout<<"layer is "<<i<<endl;
//       cout<<"numg is "<<n->getNumg()<<endl;
//    }
   
   return erorate;
}



/***************************************************************************\
**  tBedErode::SetTimeStep
**
**  Estimates a maximum time step as a function of the Courant stability
**  criterion: dt <= dx/v, where dx is the node spacing and v is the
**  wave velocity. If nb=1, v = kb Q^mb. If the equation is nonlinear
**  with nb!=1, the wave velocity is estimated as if the equation were
**  linear with a gradient term in the coefficient, ie
**      v S = [kb Q^mb S^(nb-1)] S  (recall S = -dz/dx)
**  The estimated time step limitation is therefore given by:
**      dt = 0.2 * ( dx / (kb Q^mb S^(nb-1) ) )
**  (the 0.2 is a factor to keep us comfortably below the Courant #).
**  If the denominator is zero, an arbirarily large number is returned.
**
**  Input: n -- node for which to estimate time step
**  Returns: the estimated maximum time step size
**  Assumptions: getSlope() returns a value >=0, edge length>0.
**
**  TODO: update this to handle threshold term taucd and pb
\***************************************************************************/
double tBedErodePwrLaw::SetTimeStep( tLNode * n )
{
   double slp = n->getSlope();
   if( slp < 0.0 )
       ReportFatalError("neg. slope in tBedErodePwrLaw::setTimeStep(tLNode*)");
   assert( n->getQ()>=0 );
   double eroterm = kb * pow( n->getQ(), mb ) * pow( slp, nb-1.0 );
   if( eroterm==0 ) return 100000;
   return( 0.2*n->getFlowEdg()->getLength() / eroterm );

}



/***************************************************************************\
**  FUNCTIONS FOR CLASS tSedTransPwrLaw
\***************************************************************************/

/***************************************************************************\
**  tSedTransPwrLaw Constructor:
**
**    Given a tInputFile as an argument, will read relevant parameters from
**  the input file.
**
**  Modifications:
**   - kt adjusted to include unit conversion from Q in m3/yr to shear
**     stress (sic) in SI units. See comment above for tBedErodePwrLaw
**     constructor. (GT 06/01)
**
\***************************************************************************/
tSedTransPwrLaw::tSedTransPwrLaw( tInputFile &infile )
{
   double secPerYear = 365.25*24*3600.0;  // # secs in one year

   kf = infile.ReadItem( kf, "KF" );
   kt = infile.ReadItem( kt, "KT" );
   mf = infile.ReadItem( mf, "MF" );
   nf = infile.ReadItem( nf, "NF" );
   pf = infile.ReadItem( pf, "PF" );
   tauc = infile.ReadItem( tauc, "TAUCD" );

   // Add unit conversion factor for kt -- this is required to convert
   // the quantity (Q/W)^mb from units of years to units of seconds.
   kt = kt * pow( secPerYear, -mf );
}


/***************************************************************************\
**  tSedTransPwrLaw::TransCapacity
**
**  Computes sediment transport capacity using the simple power law
**  Qs = kf W ( kt (Q/W)^mf S^nf - tauc )^pf
**
**  Modifications:
**   - Now incorporates threshold term; previously was Qs = kf Q^mf S^nf
**     (GT 5/99) (ver 2.0.2)
\***************************************************************************/
double tSedTransPwrLaw::TransCapacity( tLNode *node )
{
   double slp = node->getSlope();
   if( slp < 0.0 )
       ReportFatalError("neg. slope in tBedErodePwrLaw::TransCapacity(tLNode*)");
   double tau, tauex, cap = 0;
   if( !node->getFloodStatus() )
   {
      tau = kt * pow( node->getQ()/node->getHydrWidth(), mf ) * pow( slp, nf );
      node->setTau( tau );
      //cout << "kt=" << kt << " Q=" << node->getQ() << " W=" << node->getHydrWidth() << " S=" << node->getSlope() << endl;
      tauex = tau - tauc;
      tauex = (tauex>0.0) ? tauex : 0.0;
      cap = kf * node->getHydrWidth() * pow( tauex, pf );
      //cap = kf * pow( node->getQ(), mf ) * pow( slp, nf );
   }
   node->setQs( cap );
   return cap;
}


/***************************************************************************\
**  tSedTransPwrLaw::TransCapacity
**
**  Computes sediment transport capacity using the simple power law
**  Qs = weight kf W ( kt (Q/W)^mf S^nf - tauc )^pf
**  This is a weighted version which is called from DetachErode.
**  Weight is a weighting by depth of layer.
**  Here, qsi is set by proportion in layer and threshold is constant
**  The value returned should be in units of m^3/yr
**
**  Modifications:
**   - Now incorporates threshold term; previously was Qs = kf Q^mf S^nf
**     (GT 5/99) (ver 2.0.2)
\***************************************************************************/
double tSedTransPwrLaw::TransCapacity( tLNode *node, int lyr, double weight )
{
   double slp = node->getSlope();
   if( slp < 0.0 )
       ReportFatalError("neg. slope in tSedTransPwrLaw::TransCapacity(tLNode*)");
   double tau, tauex, cap = 0;
   
   if( !node->getFloodStatus() )
   {
      tau = kt * pow( node->getQ()/node->getHydrWidth(), mf ) * pow( slp, nf );
      node->setTau( tau );
      //cout << "kt=" << kt << " Q=" << node->getQ() << " W=" << node->getHydrWidth() << " S=" << node->getSlope() << " tau=" << tau << endl;
      tauex = tau - tauc;
      tauex = (tauex>0.0) ? tauex : 0.0;
      cap = weight * kf * node->getHydrWidth() * pow( tauex, pf );
      //cap = kf * pow( node->getQ(), mf ) * pow( slp, nf );
   }
   //cap = kf * weight * pow( node->getQ(), mf ) * pow( slp, nf );
   int i;
   for(i=0; i<node->getNumg(); i++)
       node->addQs(i, cap*node->getLayerDgrade(lyr,i)/node->getLayerDepth(lyr));
   
   /*if( node->getDrArea() > 1e7 ) {
       node->TellAll();
       cout << "tauex=" << tauex << " bfwid=" << node->getChanWidth()
            << " wid=" << node->getHydrWidth() << " cap=" << cap << endl;
            }*/
   
   
   node->setQs( cap );
   return cap;
}

   
/*************************************************************************\
**  FUNCTIONS FOR CLASS tSedTransPwrLawMulti
\**************************************************************************/

/*************************************************************************\
**  
**  tSedTransPwrLawMulti constructor
**
\**************************************************************************/
tSedTransPwrLawMulti::tSedTransPwrLawMulti( tInputFile &infile )
{
   double secPerYear = 365.25*24*3600.0;  // # secs in one year

   kf = infile.ReadItem( kf, "KF" );
   kt = infile.ReadItem( kt, "KT" );
   mf = infile.ReadItem( mf, "MF" );
   nf = infile.ReadItem( nf, "NF" );
   pf = infile.ReadItem( pf, "PF" );
   miNumgrnsizes = infile.ReadItem( miNumgrnsizes, "NUMGRNSIZE" );
   if( miNumgrnsizes>9 )
     {
       cout << "WARNING: maximum of 9 grain size classes exceeded.\n";
       cout << "Resetting to 9 size-fractions.\n";
       cout << "(That was a non-fatal warning, my friend!)\n";
       miNumgrnsizes = 9;
     }

   // Record diameter of each size-fraction
   mdGrndiam.setSize( miNumgrnsizes );
   mdTauc.setSize( miNumgrnsizes );
   std::string taglinebase = "GRAINDIAM";
   std::string digits = "123456789";
   std::string tagline;
   int i;
   double thetac = 0.045,
     sig = 2650.0,
     rho = 1000.0,
     g = 9.81;
   for( i=0; i<miNumgrnsizes; i++ )
     {
       tagline = taglinebase + digits.substr(i,i);
       mdGrndiam[i] = infile.ReadItem( mdGrndiam[i], tagline.c_str() );
       mdTauc[i] = thetac * (sig-rho) * g * mdGrndiam[i];
       //cout << "Diam " << i << " = " << mdGrndiam[i] << " tauc = " << mdTauc[i] << endl;
     }

   // Add unit conversion factor for kt -- this is required to convert
   // the quantity (Q/W)^mb from units of years to units of seconds.
   kt = kt * pow( secPerYear, -mf );

   // Read hiding/protrusion exponent (should be btwn 0 to 1)
   mdHidingexp = infile.ReadItem( mdHidingexp, "HIDINGEXP" );

}



/***************************************************************************\
**  tSedTransPwrLawMulti::TransCapacity
**
**
**  Created: 02/02 GT & RAJR
**
\***************************************************************************/
double tSedTransPwrLawMulti::TransCapacity( tLNode *node, int lyr, double weight )
{
   double slp = node->getSlope();
   if( slp < 0.0 )
       ReportFatalError("neg. slope in tSedTransPwrLaw::TransCapacity(tLNode*)");
   double d50 = 0.0,   // Mean grain size
     tau,              // Shear stress
     tauex,            // Excess shear stress 
     cap = 0;          // Xport capacity for a given size
   static tArray<double> frac( miNumgrnsizes );
   int i;
   
   // Compute D50 and fraction of each size
   for( i=0; i<miNumgrnsizes; i++ )
     {
       frac[i] = node->getLayerDgrade(lyr,i) / node->getLayerDepth(lyr);
       d50 += frac[i] * mdGrndiam[i];
       //cout << "frac " << i << " = " << frac[i] << endl;
     }
   //cout << "D50 = " << d50 << endl;

   // Compute shear stress
   if( node->getFloodStatus() ) slp = 0.0;
   tau = kt * pow( node->getQ()/node->getHydrWidth(), mf ) * pow( slp, nf );
   node->setTau( tau );
    //cout << "kt=" << kt << " Q=" << node->getQ() << " W=" << node->getHydrWidth() << " S=" << node->getSlope() << " tau=" << tau << endl;

   // Compute crit shear stress and xport capacity for each size fraction
   double totalcap = 0.0,
     tauc;
   for( i=0; i<miNumgrnsizes; i++ )
     {
       tauc = mdTauc[i] * pow( mdGrndiam[i] / d50, -mdHidingexp );
       //cout << "tauc " << i << " = " << tauc << endl;
       tauex = tau - tauc;
       tauex = (tauex>0.0) ? tauex : 0.0;
       cap = frac[i] * weight * kf * node->getHydrWidth() * pow( tauex, pf );
       totalcap += cap;
       node->addQs( i, cap );
   }
   /*if( node->getDrArea() > 1e7 ) {
       node->TellAll();*/
   //cout << "tau=" << tau << "tauex=" << tauex << " bfwid=" << node->getChanWidth()
   //<< " wid=" << node->getHydrWidth() << " cap=" << totalcap << endl;
            
   
   
   node->setQs( totalcap );
   return totalcap;
}


double tSedTransPwrLawMulti::TransCapacity( tLNode * node )
{
  return 0.0;
}


/*************************************************************************\
**  FUNCTIONS FOR CLASS tSedTransWilcock
\**************************************************************************/

/*************************************************************************\
**  
**  tSedTransWilcock constructor
**
\**************************************************************************/
tSedTransWilcock::tSedTransWilcock( tInputFile &infile )
        : grade()
{
   int i;
   char add[1], name[20];
   double help;

   cout << "tSedTransWilcock(infile)\n" << endl;
   //strcpy( add, "1" );  // GT changed from add = '1' to prevent glitch
   grade.setSize(2);
   /*for(i=0; i<=1; i++){
      strcpy( name, "GRAINDIAM");
      strcat( name, add );
      help = infile.ReadItem( help, name);
      add[0]++;
      grade[i] = help;
   }*/
   grade[0] = infile.ReadItem( grade[0], "GRAINDIAM1" );
   grade[1] = infile.ReadItem( grade[1], "GRAINDIAM2" );

   taudim= RHO*GRAV;
   refs = (RHOSED-RHO)*9.81*grade[0];
   refg = (RHOSED-RHO)*9.81*grade[1];
   lowtaucs = 0.8*(grade[1]/grade[0])*0.040*refs*0.8531;
   lowtaucg = 0.04*refg*0.8531;
   hightaucs = 0.04*refs*0.8531;
   hightaucg = 0.01*refg*0.8531;
   sands = (lowtaucs-hightaucs)/(-0.3);
   // slope = m = del y / del x
   sandb = lowtaucs-(sands*0.1);
   // intercept = y - mx
   gravs = (lowtaucg-hightaucg)/(-0.3);
   gravb = lowtaucg-(gravs*0.1);
}


/*********************************************************************\
**
**  tSedTransWilcock::TransCapacity
**
**  This function uses the sediment transport model developed by
**  P. Wilcock to calculate sediment transport rates of the sand and
**  gravel fraction individually.  This function should only be used with
**  two grain sizes and it is assumed that the grain size one is in the
**  sand range and grain size 2 is in the gravel range.  The sediment
**  transport rate of both grain sizes is calculated, and the sum of
**  these two rates is returned. (rate here is in m^3/yr)
**
**  Modifications:
**   - Reverted to earlier computation of tau using roughness and
**     width
\***********************************************************************/
#define YEARPERSEC 3.171e-8
double tSedTransWilcock::TransCapacity( tLNode *nd )
{
   double tau;
   //Xtauold;
   double taucrit;
   double persand=nd->getLayerDgrade(0,0)/(nd->getLayerDepth(0));
   //double timeadjust=31536000.00; /* number of seconds in a year */
   double factor=nd->getLayerDepth(0)/nd->getMaxregdep();

   if( nd->getSlope() < 0 ){
      nd->setQs(0, 0);
      nd->setQs(1, 0);
      nd->setQs(0);
      return 0.0;
   }

//    if(nd->getX()==12.5&nd->getY()==20){
//       cout<<"factor is "<<factor<<endl;
//    }

   // units of Q are m^3/yr; convert to m^3/sec
   //NIC you are doing a test here to see what is causing the
   //downstream coarsening.
   tau = taudim*pow(nd->getHydrRough()*nd->getQ()*YEARPERSEC/nd->getHydrWidth(), 0.6)*pow( nd->getSlope(), 0.7);
   //tau = taudim*pow(0.03, 0.6)*pow(nd->getQ()/SECPERYEAR, 0.3)*pow( nd->getSlope(), 0.7);
   
   //cout << "hydrrough is " << nd->getChanRough() << endl;
   //cout << "q is " << nd->getQ() << endl;
   //cout << "slope is " << nd->getSlope() << endl;
   //cout << "taudim is " << taudim << endl;

   //Calculate Sand transport rates first
   
   if(persand<.10)
       taucrit=lowtaucs;
   else if(persand<=.40)
       taucrit=((sands*persand)+sandb);
   else
       taucrit=hightaucs;

   //cout<<"nic value of tau is "<<tau<<" value of taucsand is "<<taucrit<<endl;
   
   if(tau>taucrit){
       nd->setQs(0, ((0.058/RHOSED)*factor*nd->getHydrWidth()*SECPERYEAR*persand*pow(tau,1.5)*pow((1-sqrt(taucrit/tau)),4.5) ));
   }
   else 
       nd->setQs( 0, 0.0 ) ;

   //Now calculate Gravel transport rates

   if(persand<.10)
       taucrit=lowtaucg;
   else if(persand<=.40)
       taucrit=((gravs*persand)+gravb);
   else
       taucrit=hightaucg;

   //cout<<"nic value of tau is "<<tau<<" value of taucgrav is "<<taucrit<<endl;

   if(tau>taucrit){
       nd->setQs(1, (0.058*SECPERYEAR*factor*nd->getHydrWidth()/(RHOSED))*
                 (1-persand)*pow(tau,1.5)*pow((1-(taucrit/tau)),4.5));
   }
   else
       nd->setQs(1,0.0);

   nd->setQs(nd->getQs(0)+nd->getQs(1));
   return nd->getQs();
       
}


/*********************************************************************\
**
**  tSedTransWilcock::TransCapacity
**
**  *nd - pointer to the node which you are calculating tranport rates at.
**  i - the layer which you are basing the transport rate on (for texture)
**  weight - used in erosion algorithm, a weight based on layer depths.
**
**  This function uses the sediment transport model developed by
**  P. Wilcock to calculate sediment transport rates of the sand and
**  gravel fraction individually.  This function should only be used with
**  two grain sizes and it is assumed that grain size one is in the
**  sand range and grain size 2 is in the gravel range.  The sediment
**  transport rate of both grain sizes is calculated, and the sum of
**  these two rates is returned. (rate here is in m^3/yr)
**  Note that this function assumes that you are looping through layers,
**  (which is why you need the weight) and so qs total and for each size
**  was initialized to zero and you just add to it in this function.
**  It is VERY IMPORTANT that qs is reset to zero before you use this
**  funtion in a loop.
\***********************************************************************/
double tSedTransWilcock::TransCapacity( tLNode *nd, int i, double weight )
{
   double tau;
   double taucrit;
   if( nd->getLayerDepth(i)<=0 ) {
      cout << "Uh-oh: i=" << i << " LD=" << nd->getLayerDepth(i) << endl;
      nd->TellAll();
   }
   assert( nd->getLayerDepth(i)>0 );
   double persand=nd->getLayerDgrade(i,0)/(nd->getLayerDepth(i));
   //double timeadjust=31536000.00; /* number of seconds in a year */
   double qss, qsg=0; //gravel and sand transport rate

   //cout<<nd->getX()<<" "<<nd->getY()<<endl;
   
   //cout << "tSedTransWilcock::TransCapacity(tLNode,int,double)\n";


   if( nd->getSlope() < 0 ){
      nd->setQs(0, 0);
      if(nd->getNumg()==2)
          nd->setQs(1, 0);
      nd->setQs(0);
      return 0.0;
   }

   // units of Q are m^3/yr; convert to m^3/sec
   //tau = taudim*pow(nd->getHydrRough()*nd->getQ()/nd->getHydrWidth(), 0.6)*pow( nd->getSlope(), 0.7);
   tau = taudim*pow(0.03, 0.6)*pow(nd->getQ()/SECPERYEAR, 0.3)*pow( nd->getSlope(), 0.7);

   //cout << "channel rough is " << nd->getChanRough() << endl;
   //cout << "channel width is " << nd->getChanWidth() << endl;
   
   //cout << "q in secs is " << nd->getQ()/SECPERYEAR << endl;
   //cout << "slope is " << nd->getSlope() << endl;
   //cout << "taudim is " << taudim << endl;

   //Calculate Sand transport rates first
   
   if(persand<.10)
       taucrit=lowtaucs;
   else if(persand<=.40)
       taucrit=((sands*persand)+sandb);
   else
       taucrit=hightaucs;

   //cout<<"nic value of tau is "<<tau<<" value of taucsand is "<<taucrit<<endl;
   //cout<<"weight = "<<weight<<" persand = "<<persand<<endl;
   
   if(tau>taucrit){
      qss=(0.058/RHOSED)*weight*nd->getHydrWidth()*SECPERYEAR*persand*pow(tau,1.5)*pow((1-sqrt(taucrit/tau)),4.5) ;
      nd->addQs(0, qss);
      //cout << "nic sand transport rate is " << qss << endl;
   }
   else 
       qss=0 ;

   //Now calculate Gravel transport rates
   if(nd->getNumg()==2){
   
      if(persand<.10)
          taucrit=lowtaucg;
      else if(persand<=.40)
          taucrit=((gravs*persand)+gravb);
      else
          taucrit=hightaucg;

      //cout<<"nic value of tau is "<<tau<<" value of taucgrav is "<<taucrit<<endl;
      
      if(tau>taucrit){
         qsg=(0.058*SECPERYEAR*weight*nd->getHydrWidth()/(RHOSED))*
             (1-persand)*pow(tau,1.5)*pow((1-(taucrit/tau)),4.5);
         nd->addQs(1,qsg);
         //cout << "gravel transport rate " <<qsg<< endl;
      }
      else
          qsg=0;
   }
   
   //NOTE - don't need to update total qs cause this gets updates
   //with update of qs of individual sizes

   return qsg+qss;
       
}

/*************************************************************************\
**  FUNCTIONS FOR CLASS tSedTransMineTailings
\**************************************************************************/

/*************************************************************************\
**  
**  tSedTransMineTailings constructor
**  
**  note that for now this is the same as tSedTransWilcock 
**  constructor since it is using same taucrit function
\**************************************************************************/
tSedTransMineTailings::tSedTransMineTailings( tInputFile &infile )
        : grade()
{
   int i;
   char add[1], name[20];
   double help;

   cout << "tSedTransMineTailings(infile)\n" << endl;
   strcpy( add, "1" );  // GT changed from add = '1' to prevent glitch
   grade.setSize(2);
   for(i=0; i<=1; i++){
      strcpy( name, "GRAINDIAM");
      strcat( name, add );
      help = infile.ReadItem( help, name);
      add[0]++;
      grade[i] = help;
   }

   taudim= RHO*GRAV;
   refs = (RHOSED-RHO)*9.81*grade[0];
   refg = (RHOSED-RHO)*9.81*grade[1];
   lowtaucs = 0.8*(grade[1]/grade[0])*0.040*refs*0.8531;
   lowtaucg = 0.04*refg*0.8531;
   hightaucs = 0.04*refs*0.8531;
   hightaucg = 0.01*refg*0.8531;
   sands = (lowtaucs-hightaucs)/(-0.3);
   // slope = m = del y / del x
   sandb = lowtaucs-(sands*0.1);
   // intercept = y - mx
   gravs = (lowtaucg-hightaucg)/(-0.3);
   gravb = lowtaucg-(gravs*0.1);
}

/*********************************************************************\
**
**  tSedTransMineTailings::TransCapacity
**
**  This function uses the sediment transport model developed on
**  mine tailings slopes in Australia by Willgoose and Riley (1998)
**  to calculate sediment transport rates of the sand and
**  gravel fraction individually.  
**
**  For now This function should only be used with
**  two grain sizes because it uses the Wilcock method to calculate taucrit.
**  It is assumed that grain size one is in the
**  sand range and grain size 2 is in the gravel range.  The sediment
**  transport rate of both grain sizes is calculated, and the sum of
**  these two rates is returned. (rate here is in m^3/yr)
**
\***********************************************************************/
#define YEARPERSEC 3.171e-8
double tSedTransMineTailings::TransCapacity( tLNode *nd )
{
   double tau;
   double taucrit;
   double persand=nd->getLayerDgrade(0,0)/(nd->getLayerDepth(0));
   //not sure what the point of factor was
   //double factor=nd->getLayerDepth(0)/nd->getMaxregdep();

   if( nd->getSlope() < 0 ){
      nd->setQs(0, 0);
      nd->setQs(1, 0);
      nd->setQs(0);
      return 0.0;
   }

//    if(nd->getX()==12.5&nd->getY()==20){
//       cout<<"factor is "<<factor<<endl;
//    }

   // units of Q are m^3/yr; convert to m^3/sec
   //tau = taudim*pow(nd->getHydrRough()*nd->getQ()*YEARPERSEC/nd->getHydrWidth(), 0.6)*pow( nd->getSlope(), 0.7);
   tau = taudim*pow(0.03, 0.6)*pow(nd->getQ()/SECPERYEAR, 0.3)*pow( nd->getSlope(), 0.7);
   
   //cout << "hydrrough is " << nd->getChanRough() << endl;
   //cout << "q is " << nd->getQ() << endl;
   //cout << "slope is " << nd->getSlope() << endl;
   //cout << "taudim is " << taudim << endl;

   //Calculate Sand transport rates first
   
   if(persand<.10)
       taucrit=lowtaucs;
   else if(persand<=.40)
       taucrit=((sands*persand)+sandb);
   else
       taucrit=hightaucs;

   //cout<<"nic value of tau is "<<tau<<" value of taucsand is "<<taucrit<<endl;
   
   if(tau>taucrit){
       nd->setQs(0,(0.0541/RHOSED)*SECPERYEAR*persand*pow(nd->getQ()/SECPERYEAR,1.12)*pow(nd->getSlope(),-0.24)*(tau-taucrit) );
   }
   else 
       nd->setQs( 0, 0.0 ) ;

   //Now calculate Gravel transport rates

   if(persand<.10)
       taucrit=lowtaucg;
   else if(persand<=.40)
       taucrit=((gravs*persand)+gravb);
   else
       taucrit=hightaucg;

   //cout<<"nic value of tau is "<<tau<<" value of taucgrav is "<<taucrit<<endl;

   if(tau>taucrit){
       nd->setQs(1, (0.0541/RHOSED)*SECPERYEAR*(1-persand)*pow(nd->getQ()/SECPERYEAR,1.12)*pow(nd->getSlope(),-0.24)*(tau-taucrit));
   }
   else
       nd->setQs(1,0.0);

   nd->setQs(nd->getQs(0)+nd->getQs(1));
   return nd->getQs();
       
}

/*********************************************************************\
**
**  tSedTransMineTailings::TransCapacity
**
**  *nd - pointer to the node which you are calculating tranport rates at.
**  i - the layer which you are basing the transport rate on (for texture)
**  weight - used in erosion algorithm, a weight based on layer depths.
**
**  This function uses the sediment transport model and parameters in
**  Willgoose and Riley (1998) to calculate sediment transport rates 
**  of the sand and gravel fraction individually.  
**  For now, this function should only be used with two grain sizes 
**  (it is using the wilcock model to calculate tau crit)
**  and it is assumed that grain size one is in the
**  sand range and grain size 2 is in the gravel range.  The sediment
**  transport rate of both grain sizes is calculated, and the sum of
**  these two rates is returned. (rate here is in m^3/yr)
**  Note that this function assumes that you are looping through layers,
**  (which is why you need the weight) and so qs total and for each size
**  was initialized to zero and you just add to it in this function.
**  It is VERY IMPORTANT that qs is reset to zero before you use this
**  funtion in a loop.
\***********************************************************************/
double tSedTransMineTailings::TransCapacity( tLNode *nd, int i, double weight )
{
   double tau;
   double taucrit;
   if( nd->getLayerDepth(i)<=0 ) {
      cout << "Uh-oh: i=" << i << " LD=" << nd->getLayerDepth(i) << endl;
      nd->TellAll();
   }
   assert( nd->getLayerDepth(i)>0 );
   double persand=nd->getLayerDgrade(i,0)/(nd->getLayerDepth(i));
   //double timeadjust=31536000.00; /* number of seconds in a year */
   double qss, qsg=0; //gravel and sand transport rate

   //cout << "tSedTransMineTailings::TransCapacity(tLNode,int,double)\n";

   if( nd->getSlope() < 0 ){
      nd->setQs(0, 0);
      if(nd->getNumg()==2)
          nd->setQs(1, 0);
      nd->setQs(0);
      return 0.0;
   }

   // units of Q are m^3/yr; convert to m^3/sec
   //NIC check to see what taudim is -> probably right but U R anal
   tau = taudim*pow(0.03, 0.6)*pow(nd->getQ()/SECPERYEAR, 0.3)*pow( nd->getSlope(), 0.7);

   /*cout << "Q is " << nd->getQ() << endl;
   cout << "slope is " << nd->getSlope() << endl;
   cout << "taudim is " << taudim << endl;
   cout << "persand is " << persand << endl;
   cout << "weight is " << weight << endl;*/

   //Calculate Sand transport rates first
   
   // here calculating critical shear stress, same as with wilcock
   if(persand<.10)
       taucrit=lowtaucs;
   else if(persand<=.40)
       taucrit=((sands*persand)+sandb);
   else
       taucrit=hightaucs;

   //cout<<"nic value of tau is "<<tau<<" value of taucsand is "<<taucrit<<endl;
   
   //remember tau is in units of sec, so compute everything in seconds
   //and transfer back to years in the end.
   //Nic, notice what you did, the commented out qss is what you originally,
   //and still kind of think is right.  Just testing.  Difference is that
   //you aren't multiply by the percentage that is there.
   //now original equation, but multiply by 10 to speed things up.
   if(tau>taucrit){
      //qss=(0.0541/RHOSED)*weight*SECPERYEAR*pow(nd->getQ()/SECPERYEAR,1.12)*pow(nd->getSlope(),-0.24)*(tau-taucrit);
      qss=(0.0541/RHOSED)*weight*SECPERYEAR*persand*pow(nd->getQ()/SECPERYEAR,1.12)*pow(nd->getSlope(),-0.24)*(tau-taucrit);
       nd->addQs(0, qss);
       //cout << "nic sand transport rate is " << qss << endl;
   }
   else{
      //cout <<"NO SAND TRANSPORT"<<endl;
      qss=0 ;
   }
   
   //Now calculate Gravel transport rates
   if(nd->getNumg()==2){
      if(persand<.10)
          taucrit=lowtaucg;
      else if(persand<=.40)
          taucrit=((gravs*persand)+gravb);
      else
          taucrit=hightaucg;

      //cout<<"nic value of tau is "<<tau<<" value of taucgrav is "<<taucrit<<endl;
      
      if(tau>taucrit){
         //qsg=(0.0541/RHOSED)*weight*SECPERYEAR*pow(nd->getQ()/SECPERYEAR,1.12)*pow(nd->getSlope(),-0.24)*(tau-taucrit);
         qsg=(0.0541/RHOSED)*weight*SECPERYEAR*(1-persand)*pow(nd->getQ()/SECPERYEAR,1.12)*pow(nd->getSlope(),-0.24)*(tau-taucrit);
         nd->addQs(1,qsg);
         //cout << "gravel transport is happening at a rate of " << qsg << endl;
      }
      else{
         //cout<<"NO GRAVEL TRANSPORT"<<endl;
         qsg=0;
      }
      
   }
   
   //NOTE - don't need to update total qs cause this gets updates
   //with update of qs of individual sizes

   return qsg+qss;
       
}

/***************************************************************************\
**  FUNCTIONS FOR CLASS tErosion
\***************************************************************************/

//constructor
tErosion::tErosion( tMesh<tLNode> *mptr, tInputFile &infile )
        : bedErode( infile ), sedTrans( infile )
{
   assert( mptr!=0 );
   meshPtr = mptr;

   // Read parameters needed from input file
   kd = infile.ReadItem( kd, "KD" );  // Hillslope diffusivity coefficient

   int optAdaptMesh = infile.ReadItem( optAdaptMesh, "OPTMESHADAPTDZ" );
   if( optAdaptMesh )
       mdMeshAdaptMaxFlux = infile.ReadItem( mdMeshAdaptMaxFlux,
                                             "MESHADAPT_MAXNODEFLUX" );

   cout << "SEDIMENT TRANSPORT OPTION: " << SEDTRANSOPTION << endl;

}


/*****************************************************************************\
**
**  tErosion::ErodeDetachLim
**
**  Solves for erosion and deposition during a time interval dtg, for the
**  case in which any sediment detached from the landscape is assumed to
**  be carried away. This might be appropriate, for example, in a bedrock
**  channel system in which deposition is negligible. This case is handled
**  separately from the more general case in which transport capacity and
**  deposition are taken into account, because the numerical solutions
**  to detachment-limited erosion equations tend to be considerably more
**  stable than the general case.
**
**  The function will solve the erosion equation(s) over one or more time
**  intervals within the total "global" time period dtg. First, the
**  maximum time step size dt is computed. Then erosion is computed
**  iteratively in increments of dt until the total time dtg has been used.
**
**  Modified, 2/20/98, SL & GT: maximize time step such that slope in down-
**   stream direction does not reverse; don't use modified Courant condition.
**   This has the advantage that it does not grind to a halt when n<1; but, it
**   is not completely satisfactory; it seems to be good at finding out how
**   small the time step needs to be but not how large it can be; i.e., if
**   dtg is quite large, one can still run into problems. E.g., for m=0.3,
**   n=0.7, kb=3.16e-02 m^3/N, U=1e-3 m/yr, Q=100m^3/s, kwds=3, knds=0.03
**   => dtg=10 yrs seems to work well; dtg=100 yrs does not work well.
**
**   - added calls to compute channel width (& depth etc) before computing
**     erosion. This is done because the detachment capacity functions now
**     require a defined channel width. (GT 2/01)
\*****************************************************************************/
void tErosion::ErodeDetachLim( double dtg, tStreamNet *strmNet )
{
  //cout<<"ErodeDetachLim...";
   double dt,
       dtmax = 1000000.0; // time increment: initialize to arbitrary large val
   double frac = 0.9; //fraction of time to zero slope
   //Xint i;
   tLNode * cn, *dn;
   int nActNodes = meshPtr->getNodeList()->getActiveSize();
   tMeshListIter<tLNode> ni( meshPtr->getNodeList() );
   tArray<double> //dz( nActNodes ), // Erosion depth @ each node
       dzdt( nActNodes ); //Erosion rate @ ea. node
   double ratediff;

   strmNet->FindChanGeom();
   strmNet->FindHydrGeom();

   cn = ni.FirstP();
   tArray<double> valgrd(1);
   //tArray<double> valgrd( cn->getNumg() );
   //TODO: make it work w/ arbitrary # grain sizes
   
   // Iterate until total time dtg has been consumed
   do
   {
      //first find erosion rate:
      for( cn = ni.FirstP(); ni.IsActive(); cn = ni.NextP() )
          cn->setDzDt( -bedErode.DetachCapacity( cn ) );

      //find max. time step s.t. slope does not reverse:
      dtmax = dtg;
      for( cn = ni.FirstP(); ni.IsActive(); cn = ni.NextP() )
      {
         dn = cn->getDownstrmNbr();
         ratediff = dn->getDzDt() - cn->getDzDt();
         if( ratediff > 0 )
         {
            dt = ( cn->getZ() - dn->getZ() ) / ratediff * frac;
            if( dt > 0.000005 && dt < dtmax ) dtmax = dt;
         }
      }
      //assert( dtmax > 0 );

      //apply erosion:
      for( cn = ni.FirstP(); ni.IsActive(); cn = ni.NextP() ){
      //	if( cn->getX()>7300.0 && cn->getX()<7400.0 && cn->getY()>500.0 && cn->getY()<600.0 )
      //	  cn->TellAll();

         //ng added stuff below to update layering using the other erodep
         //tArray<double> valgrd;
         //valgrd.setSize(1);
         valgrd[0]=cn->getDzDt() * dtmax;
         cn->EroDep( 0, valgrd, 0);
         //cn->EroDep( cn->getQs() * dtmax );
      }
      //update time:
      dtg -= dtmax;
   } while( dtg>0.0000001 );
  
}//end tErosion::ErodeDetachLim( double dtg 


/*****************************************************************************\
**
**  tErosion::ErodeDetachLim (2 of 2)
**
**  This version adds the uplift rate source term to the time-step
**  estimation. (This could be handled as a default argument, avoiding
**  the need for two nearly identical versions -- TODO)
**
**   - added calls to compute channel width (& depth etc) before computing
**     erosion. This is done because the detachment capacity functions now
**     require a defined channel width. (GT 2/01)
\*****************************************************************************/
void tErosion::ErodeDetachLim( double dtg, tStreamNet *strmNet, tUplift *UPtr )
{
   double dt,
       dtmax = 1000000.0; // time increment: initialize to arbitrary large val
   double frac = 0.1; //fraction of time to zero slope
   //Xint i;
   tLNode * cn, *dn;
   int nActNodes = meshPtr->getNodeList()->getActiveSize();
   tMeshListIter<tLNode> ni( meshPtr->getNodeList() );
   tArray<double> dzdt( nActNodes ); //Erosion rate @ ea. node
   double ratediff;
   //Xdouble dslpdt;
   double dtmin = dtg * 0.0001;

   strmNet->FindChanGeom();
   strmNet->FindHydrGeom();

   // Iterate until total time dtg has been consumed
   do
   {
      //first find erosion rate:
      for( cn = ni.FirstP(); ni.IsActive(); cn = ni.NextP() )
          cn->setDzDt( -bedErode.DetachCapacity( cn ) );
      dtmax = dtg;
      //find max. time step s.t. slope does not reverse:
      for( cn = ni.FirstP(); ni.IsActive(); cn = ni.NextP() )
      {
         /*slp = cn->getSlope();
         dslpdt = cn->getDSlopeDt();
         if( slp > 0.0 )
         {
            if( dslpdt < 0.0 )
            {
               dt = slp / (-dslpdt - UPtr->getRate() ) * frac;
               if( dt > 0 && dt < dtmax ) dtmax = dt;
            }
         }*/
         
         dn = cn->getDownstrmNbr();
         if( dn->getBoundaryFlag() == kNonBoundary )
             ratediff = dn->getDzDt() - cn->getDzDt();
         else
             ratediff = dn->getDzDt() - cn->getDzDt() - UPtr->getRate();
         if( ratediff > 0 && cn->getZ() > dn->getZ() )
         {
            dt = ( cn->getZ() - dn->getZ() ) / ratediff * frac;
            if( dt > dtmin && dt < dtmax )
            {
               dtmax = dt;
            }
            else
            {
               dtmax = dtmin;
               //cout << "time step too small because of node at x,y,z "
               //     << cn->getX() << " " << cn->getY() << " " << cn->getZ()
               //     << endl;
            }
         }
      }
      //assert( dtmax > 0 );
      //apply erosion:
      for( cn = ni.FirstP(); ni.IsActive(); cn = ni.NextP() ){
         //ng added stuff below to update layering using the other erodep
         tArray<double> valgrd;
         valgrd.setSize(1);
         valgrd[0]=cn->getDzDt() * dtmax;
         cn->EroDep( 0, valgrd, 0);
         //cn->EroDep( cn->getDzDt() * dtmax );
      }
      //update time:
      dtg -= dtmax;
   } while( dtg>0 );
   
}//end tErosion::ErodeDetachLim( double dtg, tUplift *UPtr )


/*****************************************************************************\
**
**  tErosion::StreamErode
**
**  Solution for general detachment- or transport-limited erosion. Now
**  replaced by DetachErode.
**
\*****************************************************************************/
#define kSmallTimeStep 1e-8
void tErosion::StreamErode( double dtg, tStreamNet *strmNet )
{
   double dt,
       dtmax;         // time increment: initialize to arbitrary large val
   double frac = 0.3; // fraction of time to zero slope
   //Xint i;
   tLNode * cn, *dn;
   int nActNodes = meshPtr->getNodeList()->getActiveSize();
   tMeshListIter<tLNode> ni( meshPtr->getNodeList() );
   double ratediff,  // Difference in ero/dep rate btwn node & its downstrm nbr
       cap,          // Transport capacity
       pedr,         // Potential erosion/deposition rate
       dcap,         // Bedrock detachment capacity
       dz,           // Depth of deposition/erosion (erosion = negative)
       dzr;          // Potential depth of bedrock erosion
   int smallflag=0, smallcount=0;

   cout << "tErosion::StreamErode\n";

   // Sort so that we always work in upstream to downstream order
   strmNet->SortNodesByNetOrder();

   // Compute erosion and/or deposition until all of the elapsed time (dtg)
   // is used up
   do
   {
      // Zero out sed influx
      for( cn = ni.FirstP(); ni.IsActive(); cn = ni.NextP() )
          cn->setQsin( 0.0 );

      // Compute erosion rates: when this block is done, the transport rate
      // (qs), influx (qsin), and deposition/erosion rate (dzdt) values are
      // set for each active node.
      for( cn = ni.FirstP(); ni.IsActive(); cn = ni.NextP() )
      {
         // Transport capacity and potential erosion/deposition rate
         // (this also sets the node's Qs value)
         cap = sedTrans.TransCapacity( cn );
         pedr = (cn->getQsin() - cap ) / cn->getVArea();
        //sediment input:
         if( cn == strmNet->getInletNodePtr() )
             pedr += strmNet->getInSedLoad() / cn->getVArea();
         
         // If we're on bedrock, adjust accordingly
         if( cn->OnBedrock() && pedr<0 )
         {
            // get detachment capacity (this also sets node's drdt)
            dcap = -bedErode.DetachCapacity( cn );
            if( dcap > pedr )
                pedr = dcap;
         }
         // set the erosion (deposition) rate and send the corresponding
         // sediment influx downstream
         cn->setDzDt( pedr );
         cn->getDownstrmNbr()->addQsin( cn->getQsin() - pedr*cn->getVArea() );
         //cout << "RATE STEP:\n";
         //cn->TellAll();
      }

      // Given these rates, figure out how big a time-step we can get away with
      //  (Note: the division and subsequent multiplication by frac are done
      //   for performance reasons: we avoid having to multiply every dt value
      //   by frac)
      dtmax = dtg/frac;
      for( cn = ni.FirstP(); ni.IsActive(); cn = ni.NextP() )
      {
         dn = cn->getDownstrmNbr();
         ratediff = dn->getDzDt() - cn->getDzDt(); // Are the pts converging?
         if( ratediff > 0 && cn->getZ() > dn->getZ() )  // if yes, get time
         {                                              //  to zero slope
            dt = ( cn->getZ() - dn->getZ() ) / ratediff;
            if( dt < dtmax ) dtmax = dt;
            /*if( dt < kSmallTimeStep )
            {
               cout << "Very small dt " << dt << " at:\n";
                cn->TellAll();
                dn->TellAll();
            }*/
            
         }
      }
      dtmax *= frac;  // Take a fraction of time-to-flattening
      if( dtmax < kSmallTimeStep ) dtmax = kSmallTimeStep;
      if( dtmax <= 0.01 && smallflag==0 )
      {
         smallflag=1;
         cout << "SMALL STEP: " << dtmax << endl;
      }
      if( smallflag==1 )
      {
         smallcount++;
         if( smallcount==100 )
         {
            cout << "TIME REMAINING: " << dtg << endl;
            smallcount=0;
         }
      }
      
      //cout << "  dt " << dtmax << endl;

      // Zero out sed influx again, because depending on bedrock-alluvial
      // interaction it may be modified; if inlet, give it strmNet->inlet.inSedLoad
      for( cn = ni.FirstP(); ni.IsActive(); cn = ni.NextP() )
          cn->setQsin( 0.0 );
        //sediment input:
      if(strmNet->getInletNodePtrNC() != NULL)
	{
          strmNet->getInletNodePtrNC()->setQsin( strmNet->getInSedLoad() );
	  cout << "Inlet node:\n";
	  strmNet->getInletNodePtr()->TellAll();
	}

      // Notes for multi-size adaptation:
      // qs, qsin, dz, etc could be arrays with dimensions (1..NUMG+1) with
      // the extra entry storing the total. "on bedrock" might be defined as
      // active layer depth less than its "normal" depth (even zero).
      // For bedrock, crit shear might become maximum because of protrusion
      // over the bed --- how to handle?
      // Bedrock scour could automatically generate a given distribution of
      // sizes (specified as a parameter)
      // The basic rule here, as in golem, is don't erode more bedrock than
      // you have capacity to carry. but what happens when you have plenty of
      // surplus capacity in one size and none (or deposition) in another?
      // Could use total capacity: limit TOTAL br erosion to that allowed by
      // TOTAL excess capacity. If some material is generated that can't be
      // carried, just put it in the active layer.
      // One way to proceed would be to _always_ have scour when the bed is
      // exposed, and count the scoured material as part of the sediment
      // influx. If the influx is too large for the avail capacity, just
      // leave it in the active layer. Probably ok as long as time steps
      // remain small and br erosion rate isn't too large relative to capacity.
      // In this case, algo might be:
      // - if on bedrock, erode br and "inject" resulting sed into Qsin,
      //   remembering depth of br erosion
      // - compute surplus/deficit capacity in each size frac. If erosion
      //   amount in a size exceed AL thickness, limit it.
      // - do mass balance on each size to get dz for each size and total
      // - apply br erosion depth
      // this would mean you could have simultaneous deposition _and_ br
      // erosion, but that's not totally unreasonable and would be unlikely
      // to last very long.
      
      // Now do erosion/deposition by integrating rates over dtmax
      for( cn = ni.FirstP(); ni.IsActive(); cn = ni.NextP() )
      {
         // Depth of potential erosion due to excess transport capacity
         // Note: for multiple sizes, dz could be an array (1..NUMG),
         // maybe w/ an extra field for the total dz.
         dz = ( (cn->getQsin() - cn->getQs() ) / cn->getVArea() ) * dtmax;
         
         // If we're on bedrock, scour the bedrock
         if( cn->OnBedrock() && dz<0.0 ) {
            dzr = cn->getDrDt()*dtmax;  // potential bedrock erosion depth
            // If excess capacity depth-equivalent is greater than the depth
            // of sediment available on bed plus eroded from bedrock, limit
            // depth of erosion to alluvium plus bedrock erosion depth
            if( -dz > -dzr+cn->getAlluvThickness() ) {
               dz = dzr-cn->getAlluvThickness(); 
            }
         }
         
         //cout << "** THIS node has dz " << dz << endl << flush;
         //cn->TellAll();

         // Update alluvium thickness and node elevation
         /*if( cn->getID()==3214 )
         {
             cn->TellAll();
             cout << "Applying dz=" << dz << endl;
         }
         if( cn->getZ() < -1 ) {
            cout << "The following node is going below baselevel:\n";
            cn->TellAll();
         }*/
         cn->EroDep( dz );
         dn = cn->getDownstrmNbr();
         /*if( dn->getID()==3214 )
         {
            cout << "SENDing to 3214: " << cn->getQsin() << " - " << dz*cn->getVArea()/dtmax << endl;
            cn->TellAll();
         }*/

         // Send sediment downstream: sediment flux is equal to the flux in
         // plus/minus rate of erosion/deposition times node area
         assert( dtmax>0 );
         dn->addQsin( cn->getQsin() - dz*cn->getVArea()/dtmax );
//           if( (cn->getQsin() - dz*cn->getVArea()/dtmax) < -0.1 )
//           {
//              cout << "NEG OUTFLUX! (dz=" << dz << ")\n";
//              cout << "outflux: " << cn->getQsin() - dz*cn->getVArea()/dtmax
//                   << endl;
//              cn->TellAll();
//              dn->TellAll();
//           }
      }

      // Update time remaining
      dtg -= dtmax;
      
   } while( dtg>1e-6 ); // Keep going until we've used up the whole time intrvl

   //cout << "Leaving StreamErode()\n";
}


/***************************************************************************\
**  tErosion::StreamErodeMulti
**
**  Eventually there should only be one streamerode (not one for multi
**  and one for single size).  This work is still in progress so both
**  remain for the time being.  This function calculates the erosion and
**  deposition rate of each grain size assuming the system is transport
**  limited.  If there is less than a critical amount of sediment on the
**  bed, the bedrock is eroded and added to the load.
**  ...NOW OBSOLETE?
\***************************************************************************/
void tErosion::StreamErodeMulti( double dtg, tStreamNet *strmNet, double time )
{
   double dt,
       dtmax;         // time increment: initialize to arbitrary large val
   double frac = 0.3; //fraction of time to zero slope
   double timegb; //time gone by - for layering time purposes
   int i;
   tLNode * cn, *dn;
   int nActNodes = meshPtr->getNodeList()->getActiveSize();
   tMeshListIter<tLNode> ni( meshPtr->getNodeList() );
   double ratediff,  // Difference in ero/dep rate btwn node & its downstrm nbr
       cap,
       pedr,
       dcap,
       dzt, // Total amount of sediment erosion
       dzrt; // Total amount of bedrock erosion
   
   cn = ni.FirstP();
   
   tArray <double> dz( cn->getNumg() );
   // Depth of deposition/erosion  of each size (erosion = negative)
   tArray <double> dzr( cn->getNumg() );
   // Potential depth of bedrock erosion of each grain size
   tArray <double> retbr( cn->getNumg() ); //br amt actually ero'd/dep'd
   tArray <double> retsed( cn->getNumg()  ); //sed amt actually ero'd/dep'd

   // Sort so that we always work in upstream to downstream order
   strmNet->SortNodesByNetOrder();
   strmNet->FindChanGeom();
   strmNet->FindHydrGeom();

   // Compute erosion and/or deposition until all of the elapsed time (dtg)
   // is used up
   timegb=time;
   do
   {
      //cout<<"AT BEGINNING!"<<endl;
      // Zero out sed influx of all sizes
      for( cn = ni.FirstP(); ni.IsActive(); cn = ni.NextP() ){
         //if(cn->getID()==93)
         //  cout<<"93 is active"<<endl;
         cn->setQsin(0.0); //totals are for ts calculation
         cn->setQs(0.0);
         for( i=0; i<cn->getNumg(); i++ ){
             cn->setQsin( i, 0.0 );
             cn->setQs(i,0.0);
         }
         //if(cn->getID()==93)
         //    cout<<"93 is active"<<endl;
      }

      // Compute erosion rates: when this block is done, the transport rates
      // (qsm), influx (qsinm), and deposition/erosion rate (dzdt) values are
      // set for each active node.
      for( cn = ni.FirstP(); ni.IsActive(); cn = ni.NextP() )
      {
         // Transport capacity and potential erosion/deposition rate
         // (this also sets the node's Qs value)
         // remember that qs and qsin store totals of qsm and qsinm, resp.
         if(cn->getLayerSed(0)>0)
         {
            // Sediment in first layer so transport cap
            cap = sedTrans.TransCapacity( cn );
            pedr = (cn->getQsin() - cap ) / cn->getVArea();
         
            //sediment input:
            if( cn == strmNet->getInletNodePtr() ){
               pedr += strmNet->getInSedLoad() / cn->getVArea();
            }
            // If we're on bedrock, adjust accordingly
            if( cn->getLayerSed(1)==0 && pedr<0 &&  fabs(cn->getLayerDepth(0)-cn->getMaxregdep())>0.001 )
            {
               // Bedrock right below sediment so erode this
               // Get detachment capacity (this also sets node's drdt)
               // Limit detachment because there is also sediment present
               dcap = -bedErode.DetachCapacity(cn)*(1-(cn->getLayerDepth(0)/cn->getMaxregdep()));
               pedr += dcap;
               // if(fabs(dcap)>1){
//                   cout << "huge bedrock erosion of "<< dcap << " at node " << cn->getID()<<endl;
//                }
               
            }
            
         }
         else
         {
            // Top layer is bedrock, so just detach and go...
            //sediment input:
            // ignoring sediment input for now
            // nic - NEED TO CHANGE THIS
            //if( cn == strmNet->getInletNodePtr() ){
            //   pedr += strmNet->getInSedLoad() / cn->getVArea();
            //}
            //if( pedr<0 )
            //{
               // Get detachment capacity (this also sets node's drdt)
             dcap = -bedErode.DetachCapacity( cn );
             // if(fabs(dcap)>1){
//                 cout << "huge bedrock erosion of "<< dcap <<" at node " << cn->getID()<< endl;
//              }
                          
             //if( dcap > pedr )
                   pedr = dcap;
                   //}
         }

         // if(cn->getX()==2.75 && cn->getY()==1){
//             cout<<cn->getNumLayer()<<" layers" <<endl;
//             n=0;
//             while(n<cn->getNumLayer()){
//                cout << "layer " << n+1 << endl;
//                cout << "layer creation time is " << cn->getLayerCtime(n) << endl;
//                cout << "layer recent time is " << cn->getLayerRtime(n) << endl;
//                cout << "layer depth is " << cn->getLayerDepth(n) << endl;
//                cout << "layer erodibility is " << cn->getLayerErody(n) << endl;
//                cout << "is layer sediment? " << cn->getLayerSed(n) << endl;
//                cout << "dgrade 1 is " << cn->getLayerDgrade(n,0) << endl;
//                cout << "dgrade 2 is " << cn->getLayerDgrade(n,1) << endl;
//                n++;  
//             }
//             cout<<"qs0 is "<<cn->getQs(0)<<" qs1 is "<<cn->getQs(1)<<endl;
//             cout<<"texture of surface is "<<cn->getLayerSed(0)<<endl;
//          }
         
         // Set the erosion (deposition) rate and send the corresponding
         // sediment influx downstream
         cn->setDzDt( pedr );
         cn->getDownstrmNbr()->addQsin( cn->getQsin() - pedr*cn->getVArea() );
         // only doing totals here
         // sediment transport rates for each grn size have been calculated
         //cout << "RATE STEP:\n";
         //cn->TellAll();
      }
      
      

      // Given these rates, figure out how big a time-step we can get away with
      //  (Note: the division and subsequent multiplication by frac are done
      //   for performance reasons: we avoid having to multiply every dt value
      //   by frac)
      dtmax = dtg/frac;
      for( cn = ni.FirstP(); ni.IsActive(); cn = ni.NextP() )
      {
         dn = cn->getDownstrmNbr();
         ratediff = dn->getDzDt() - cn->getDzDt(); // Are the pts converging?
         if( ratediff > 0 && cn->getZ() > dn->getZ() )  // if yes, get time
         {                                              //  to zero slope
            dt = ( cn->getZ() - dn->getZ() ) / ratediff;
            if( dt < dtmax ) dtmax = dt;
            if( dt < 1e-6 )
            {
               
               cout << "Very small dt " << dt << " at:\n" << endl;
               //cout << "rate dif is " << ratediff << endl;
               //cout << "elev dif is " << cn->getZ() - dn->getZ() << endl;
               //cout << "dzdt upstream is " << cn->getDzDt() << endl;
               //cout << "dzdt downstream is " << dn->getDzDt() << endl;
               // cn->TellAll();
               //dn->TellAll();
               //cout << "arbitrarily set dt to 0.0015" << endl;
               dtmax=0.005;
            }
         }
      }
      dtmax *= frac;  // Take a fraction of time-to-flattening
      
      //cout<<"dtmax is "<<dtmax<<endl;
      
      // Zero out sed influx again, because depending on bedrock-alluvial
      // interaction it may be modified; if inlet, give it strmNet->inlet.inSedLoad
      for( cn = ni.FirstP(); ni.IsActive(); cn = ni.NextP() ){
         cn->setQsin(0.0);
         for( i=0; i<cn->getNumg(); i++ )
             cn->setQsin( i, 0.0 );
      }
      

      //sediment input:
      // nic- ignoring sediment input for now CHANGE THIS LATER
      //if(strmNet->getInletNodePtrNC() != NULL){
      //   for( i=0; i<=cn->getNumg(); i++ )
      //      strmNet->getInletNodePtrNC()->setQsin(i,strmNet->getInSedLoad(i) );
      //}
      

      // Notes for multi-size adaptation:
      // qs, qsin, dz, etc could be arrays with dimensions (1..NUMG+1) with
      // the extra entry storing the total. "on bedrock" might be defined as
      // active layer depth less than its "normal" depth (even zero).
      // For bedrock, crit shear might become maximum because of protrusion
      // over the bed --- how to handle?
      // Bedrock scour could automatically generate a given distribution of
      // sizes (specified as a parameter)
      // The basic rule here, as in golem, is don't erode more bedrock than
      // you have capacity to carry. but what happens when you have plenty of
      // surplus capacity in one size and none (or deposition) in another?
      // Could use total capacity: limit TOTAL br erosion to that allowed by
      // TOTAL excess capacity. If some material is generated that can't be
      // carried, just put it in the active layer.
      // One way to proceed would be to _always_ have scour when the bed is
      // exposed, and count the scoured material as part of the sediment
      // influx. If the influx is too large for the avail capacity, just
      // leave it in the active layer. Probably ok as long as time steps
      // remain small and br erosion rate isn't too large relative to capacity.
      // In this case, algo might be:
      // - if on bedrock, erode br and "inject" resulting sed into Qsin,
      //   remembering depth of br erosion
      // - compute surplus/deficit capacity in each size frac. If erosion
      //   amount in a size exceed AL thickness, limit it.
      // - do mass balance on each size to get dz for each size and total
      // - apply br erosion depth
      // this would mean you could have simultaneous deposition _and_ br
      // erosion, but that's not totally unreasonable and would be unlikely
      // to last very long.

      timegb += dtmax;
      // Now do erosion/deposition by integrating rates over dtmax
      for( cn = ni.FirstP(); ni.IsActive(); cn = ni.NextP() )
      {
         // Depth of potential erosion due to excess transport capacity
         dzt=0;
         //cn->TellAll();
         for( i=0; i<cn->getNumg(); i++ ){
            dz[i] = ( (cn->getQsin(i)-cn->getQs(i)) / cn->getVArea() ) * dtmax;
            dzt += dz[i];
            retbr[i]=0;
            retsed[i]=0;
         }
        //  if(cn->getX()==2.75 && cn->getY()==1){
//             cout<<"qsin0 qs0 "<<cn->getQsin(0)<<" "<<cn->getQs(0)<<endl;
//             cout<<"qsin1 qs1 "<<cn->getQsin(1)<<" "<<cn->getQs(1)<<endl;
//             cout<<"area "<<cn->getVArea()<<" time "<<dtmax<<endl;
//          }
         
         
         // If we're on bedrock, scour the bedrock
         dzrt = 0;
         if(cn->getLayerSed(0)<1){
            // Bedrock at surface
            for( i=0; i<cn->getNumg(); i++ ){
               dzr[i] = cn->getDrDt()*cn->getLayerDgrade(0,i)/cn->getLayerDepth(0)*dtmax;
               dzrt += dzr[i];
               // if(cn->getX()==2.75 && cn->getY()==1){
//                   cout<<"no sed drdt is "<<dzrt<<" of node "<<cn->getID()<<endl;
//                   cout<<"should erode some bedrock"<<endl;
//                }
            }
            
            if(dzrt<0)
                retbr=cn->EroDep(0,dzr,timegb);
            //if(cn->getID()==93){
            // cout<<"done with erosion nic"<< endl;
            //}
            
         }
         else if( fabs(cn->getLayerDepth(0)-cn->getMaxregdep())>0.001 && dzt<0.0 && cn->getLayerSed(1)<1)
         {
            // Bedrock not at surface, but not enough sediment either
            // nic this should work if only regolith and bedrock
            // and also IF layering is working correctly
            for( i=0; i<cn->getNumg(); i++ ){
               dzr[i] = cn->getDrDt()*cn->getLayerDgrade(1,i)/cn->getLayerDepth(1)*dtmax*((cn->getMaxregdep()-cn->getLayerDepth(0))/cn->getMaxregdep());
               dzrt += dzr[i];
               // potential bedrock erosion depth
               // If excess capacity depth-equivalent is greater than the depth
               // of sediment available on bed plus eroded from bedrock, limit
               // depth of erosion to alluvium plus bedrock erosion depth
               //if( -dz[i] > -dzr[i]+cn->getAlluvThickness( i ) )
               //  dz[i] = dzr[i]+cn->getAlluvThickness( i );
               // nic - don't think you need this
            }
            // if(cn->getX()==2.75 && cn->getY()==1){
//                cout<<"sed on top drdt is "<<dzrt<<" of node "<<cn->getID()<<endl;
//                cout<<"should erode some bedrock"<<endl;
//             }
            
            if(dzrt<0)
                retbr=cn->EroDep(1,dzr,timegb);
         }
                  
         //cout << "** THIS node has dzs " << dzs << " & dzr " << dzr
         //     << " & net change " << dzr+dzs << endl;
         //cn->TellAll();

         // Update alluvium thickness and node elevation
         if(fabs(dzt)>0){
            // if(cn->getX()==2.75 && cn->getY()==1){
//                 cout<<"eroding sediment at specified node, total of "<<dzt<<endl;
//                 cout<<"Total number of layers is "<<cn->getNumLayer()<<endl;
//                 cout<<"dz0 is "<<dz[0]<<" dz1 is "<<dz[1]<<endl;
                
//             }
            
            //cout << "dzt is " << dzt << endl;
            retsed=cn->EroDep(0, dz, timegb );
         }
         
         dn = cn->getDownstrmNbr();

         // Send sediment downstream: sediment flux is equal to the flux in
         // plus/minus rate of erosion/deposition times node area
         for(i=0; i<cn->getNumg(); i++){
            dn->addQsin( i, cn->getQsin(i) - (retbr[i]+retsed[i])*cn->getVArea()/dtmax );
         }
         //cn->TellAll();
      }
      
      

      // Update time remaining
      dtg -= dtmax;
   } while( dtg>1e-6 ); // Keep going until we've used up the whole time intrvl
   
}


/***********************************************************************\
**
**  tErosion::DetachErode
**
**  Algorithm for eroding sediment and bedrock.  Material is only detached
**  if the stream has the capacity to carry it. Handles multiple grain
**  sizes. Replaces StreamErode and StreamErodeMulti.
**
\************************************************************************/

void tErosion::DetachErode(double dtg, tStreamNet *strmNet, double time )
{

   //Added 4/00, if there is no runoff, this would crash, so check
   if(strmNet->getRainRate()-strmNet->getInfilt()>0){

      double dt,
          dtmax;          // time increment: initialize to arbitrary large val
      double frac = 0.3;  //fraction of time to zero slope
      double timegb=time; //time gone by - for layering time purposes
      int i,j, flag;
      tLNode * cn, *dn;
      int nActNodes = meshPtr->getNodeList()->getActiveSize();
      tMeshListIter<tLNode> ni( meshPtr->getNodeList() );
      double ratediff,  // Difference in ero/dep rate btwn node & its downstrm nbr
          drdt,
          dz,
          depck,
          qs,
          excap,
          sum;
      tLNode * inletNode = strmNet->getInletNodePtr();
      double insedloadtotal = strmNet->getInSedLoad();
      
      cn = ni.FirstP();
      
      tArray <double> ret( cn->getNumg() ); //amt actually ero'd/dep'd
      tArray <double> erolist( cn->getNumg() );
      tArray <double> insed = strmNet->getInSedLoadm();
      
      // Sort so that we always work in upstream to downstream order
      strmNet->SortNodesByNetOrder();
      strmNet->FindChanGeom();
      strmNet->FindHydrGeom();
      
      
      // Compute erosion and/or deposition until all of the elapsed time (dtg)
      // is used up
      do
      {   
         // Zero out sed influx of all sizes
         for( cn = ni.FirstP(); ni.IsActive(); cn = ni.NextP() )
         {
            cn->setQs(0.0);
            if( cn!=inletNode )
            {
               cn->setQsin(0.0); //totals are for ts calculation
               for( i=0; i<cn->getNumg(); i++ ){
                  cn->setQsin( i, 0.0 );
                  cn->setQs(i,0.0);
               }
            }
            else
            {
               cn->setQsin(insedloadtotal); //totals are for ts calculation
               for( i=0; i<cn->getNumg(); i++ ){
                  cn->setQs(i,0.0);
                  cn->setQsin( i, insed[i] );
		  //cout << "inlet qsin size " << i << "=" << insed[i] << endl;
               }
            }
         }
         
         // Estimate erosion rates and time-step size
         // NOTE - in this first loop we are only dealing with
         // totals for time-step calculations, however transport
         // rates for each size are also set within the function call.
         for( cn = ni.FirstP(); ni.IsActive(); cn = ni.NextP() )
         {
            depck=0;
            i=0;
            drdt=0;
            qs=0;
            
            assert(cn->getChanDepth()<1000);
            
            while((cn->getChanDepth()-depck)>0.0001)
            {
               // Total transport capacity is a weighted average
               // of the transport capacity calculated from each 
               // layer within the channel depth.
               // sediment and bedrock treated the same
               // units on qs are l^3/t
               if((depck+cn->getLayerDepth(i))<=cn->getChanDepth()){
                  //TransportCapacity function should keep running
                  //sum of qs of each grain size.  
                  //qs returned is in m^3/yr; qs stored in tLNode has same units
                  qs+=sedTrans.TransCapacity(cn,i,cn->getLayerDepth(i)/cn->getChanDepth());
               }
               else{
                  qs+=sedTrans.TransCapacity(cn,i,1-(depck/cn->getChanDepth()));
               }
               depck+=cn->getLayerDepth(i); //need to keep this here for qs calc
               i++;
            }

            //NIC this detachcapacity returns the correct thing, but
            //it also sets within the layer the drdt of each size.
            //You don't want to use detach capacity this way, so
            //I don't think that will affect anything, just be careful of
            //using those values!!!
            
            if(depck>cn->getChanDepth()) //which layer are you basing detach on?
                drdt=-bedErode.DetachCapacity( cn, i-1 );
            else
                drdt=-bedErode.DetachCapacity( cn, i );//[m^3/yr]
            
            cn->setDrDt(drdt);
            cn->setDzDt(drdt);
            
	    /*cout << "*** EROSION ***\n";
	    if( cn->getID()==342 ) {
	      cout << "**** Trans Cap 342 = " << qs << endl;
	      cn->TellAll();
	    }*/
            
            excap=(qs - cn->getQsin())/cn->getVArea();//[m/yr]
            //excap negative = deposition; positive = erosion
            //Note that signs are opposite to what one
            //might expect.  This works out for Qsin addition.
            //Limit erosion to capacity of flow or deposition
            if( -drdt > excap ){
               cn->setDzDt(-excap);
            }
            cn->getDownstrmNbr()->addQsin(cn->getQsin()-cn->getDzDt()*cn->getVArea());
         }//ends for( cn = ni.FirstP...
         
         //Find local time-step based on dzdt
         dtmax = dtg/frac;
         for( cn = ni.FirstP(); ni.IsActive(); cn = ni.NextP() )
         {
            //Not for time step calculations, just utilizing loop
            if( cn!=inletNode )
            {
               //Note-seting qsini for each size should automatically
               //properly set qstotal to be the sum of all qsini
               for( i=0; i<cn->getNumg(); i++ )
                   cn->setQsin( i, 0.0 );
            }
            else
            {
               for( i=0; i<cn->getNumg(); i++ )
                   cn->setQsin( i, insed[i] );
            }
            
            dn = cn->getDownstrmNbr();
            ratediff = dn->getDzDt() - cn->getDzDt(); //Are the pts converging?
            if( ratediff > 0 && (cn->getSlope()) > 1e-7 )  // if yes, get time
            {                                              //  to zero slope
               dt = ( cn->getZ() - dn->getZ() ) / ratediff;
               if( dt < dtmax ) dtmax = dt;
               if( dt < 0.0001 )
               {
                  //cout << "Very small dt " << dt <<  endl;
                  //cout << "rate dif is " << ratediff << endl;
                  //cout << "elev dif is " << cn->getZ() - dn->getZ() << endl;
                  //cout << "dzdt upstream is " << cn->getDzDt() << endl;
                  //cout << "dzdt downstream is " << dn->getDzDt() << endl;
                  //cn->TellAll();
                  //dn->TellAll();
                  //cout << "arbitrarily set dt to 0.0015" << endl;
                  dtmax=0.0001; //GREG I added this just because things
                  // were taking forever.  I kept it for now just for
                  // testing stuff.  Maybe we should discuss this.
               }
            }
         }// End for( cn = ni.FirstP()..
         dtmax *= frac;  // Take a fraction of time-to-flattening
         timegb+=dtmax;
         
         //At this point: we have drdt and qs for each node, plus dtmax
         
         // Do erosion/deposition
         for( cn = ni.FirstP(); ni.IsActive(); cn = ni.NextP() )
         {
            //need to recalculate cause qsin may change due to time step calc
            excap=(cn->getQs() - cn->getQsin())/cn->getVArea();         
            
            //cout<<"actual erosion excap = "<<excap<<endl;
            //cout<<"drdt is "<<cn->getDrDt()<<endl;
            //again, excap pos if eroding, neg if depositing
            //nic here is where drdt comes in again
            //flag is used to determine the texture of what should be eroded.
            //If detach limited, just erode what is there, but always limit
            //it by what flow has capacity to transport.  If transport limited,
            //the texture of what erode is determined by the calculated values
            //of qs.
            dz=0;
            if( -cn->getDrDt() < excap ){
               dz = cn->getDrDt()*dtmax; // detach-lim
               flag = 0;
            }
            else{
               dz = -excap*dtmax; // trans-lim
               flag = 1;
            }
            
            for(i=0; i<cn->getNumg(); i++)
                cn->getDownstrmNbr()->addQsin(i,cn->getQsin(i));
            //What goes downstream will be what comes in + what gets ero'd/dep'd
            //This should always be negative or zero since max amt
            //to deposit is what goes in.
            //i.e. send (qsin[i]-ret[i]*varea/dtmax) downstream
            //Note: I think need to do the add in here and possibly take out later
            //because of looping through layers for the same erosion pass.
            
            /*DEBUG double l0, l1;
              if( cn->getX()>50.0 && cn->getX()<51.0 
              && cn->getY()>29.0 && cn->getY()<30.0 )
              {
              cout << "f (" << cn->getID() << " ld = " << cn->getLayerDepth(0) << endl;
              l0 = cn->getLayerDgrade(0,0);
              l1 = cn->getLayerDgrade(0,1);
              }*/
            
            if( dz<0 ) //total erosion
            {
               if(flag==0){ // detach-lim
                  i=0;
                  depck=0;
                  while(dz<-0.000000001&&depck<cn->getChanDepth()&&i<cn->getNumLayer()){   
                     depck+=cn->getLayerDepth(i);
                     if(-dz<=cn->getLayerDepth(i)){//top layer can supply total depth
                        for(j=0;j<cn->getNumg();j++){
                           erolist[j]=dz*cn->getLayerDgrade(i,j)/cn->getLayerDepth(i);
                           if(erolist[j]<(cn->getQsin(j)-cn->getQs(j))*dtmax/cn->getVArea()){
                              //decrease total dz because of capacity limitations
                              erolist[j]=(cn->getQsin(j)-cn->getQs(j))*dtmax/cn->getVArea();
                              cn->setQsin(j,0.0);
                              cn->setQs(j,0.0);
                           }
                        }
                        ret=cn->EroDep(i,erolist,timegb);
                        for(j=0;j<cn->getNumg();j++){
                           cn->getDownstrmNbr()->addQsin(j,-ret[j]*cn->getVArea()/dtmax);
                        }
                        dz=0;
                     }
                     else{//top layer is not deep enough, need to erode more layers
                        sum=0;
                        flag=0;
                        for(j=0;j<cn->getNumg();j++){
                           erolist[j]=-cn->getLayerDgrade(i,j);
                           if(erolist[j]<(cn->getQsin(j)-cn->getQs(j))*dtmax/cn->getVArea()){
                              //decrease total dz because of capacity limitations
                              erolist[j]=(cn->getQsin(j)-cn->getQs(j))*dtmax/cn->getVArea();
                              cn->setQsin(j,0.0);
                              cn->setQs(j,0.0);
                              //need to set these to zero since the capacity has
                              //now been filled by the stuff in this layer
                              flag=1;
                              //Since not taking all of the material from the
                              //surface, surface layer won't be removed-must inc i
                           }
                           dz-=erolist[j];
                        }
                        ret=cn->EroDep(i,erolist,timegb);
                        for(j=0;j<cn->getNumg();j++){
                           //if * operator was overloaded for arrays, no loop necessary
                           cn->getDownstrmNbr()->addQsin(j,-ret[j]*cn->getVArea()/dtmax);
                        }
                        if(flag==1){
                           i++;
                        }
                     }
                  }
               }
               else{//trans-lim
                  //cout<<"X "<<cn->getX()<<" Y "<<cn->getY();
                  for(j=0;j<cn->getNumg();j++){
                     erolist[j]=(cn->getQsin(j)-cn->getQs(j))*dtmax/cn->getVArea();
                     // cout<<" j "<<j<<" "<<erolist[j];
                  }
                  //cout<<endl;
                  
                  i=0;
                  depck=0;
                  while(depck<cn->getChanDepth()){
                     depck+=cn->getLayerDepth(i);
                     flag=cn->getNumLayer();
                     ret=cn->EroDep(i,erolist,timegb);
                     sum=0;
                     for(j=0;j<cn->getNumg();j++){
                        cn->getDownstrmNbr()->addQsin(j,-ret[j]*cn->getVArea()/dtmax);
                        erolist[j]-=ret[j];
                        sum+=erolist[j];
                     }
                     if(sum>-0.0000001)
                         depck=cn->getChanDepth();
                     if(flag==cn->getNumLayer())
                         i++;
                  }
               }//end if( trans-limited )
            }//ends(if dz<0)
            else if(dz>0) //total deposition -> need if cause erodep chokes with 0
            {
               //Get texture of stuff to be deposited
               for(j=0;j<cn->getNumg();j++)
                   erolist[j]=(cn->getQsin(j)-cn->getQs(j))*dtmax/cn->getVArea();
               ret=cn->EroDep(0,erolist,timegb);
               for(j=0;j<cn->getNumg();j++){
                  cn->getDownstrmNbr()->addQsin(j,-ret[j]*cn->getVArea()/dtmax);
               }
               
            }
            
         } // Ends for( cn = ni.FirstP()...
         // Update time remainig   
         dtg -= dtmax;
         //cout<<"Time remaining now "<<dtg<<endl;
      } while( dtg>1e-6 );  //Keep going until we've used up the whole time intrvl
   }//end if rainrate-infilt>0
   

   //cout<<"ending detach erode"<<endl;
}// End erosion algorithm


/*****************************************************************************\
 **
 **  tErosion::Diffuse
 **
 **  Computes slope-dependent mass transfer by hillslope creep-related
 **  processes (hillslope diffusion). Volumetric flux of sediment occurs
 **  across each Voronoi cell face; the volume rate of sediment transfer
 **  between two nodes that share a Voronoi face of length Lv is
 **    Fv = Kd * S * Lv,
 **  with transfer being in the downhill (positive S) direction. The total
 **  transfer during a time step dt is Fv*dt.
 **    Because transfer occurs along edges (across Voronoi faces), we can
 **  compute the solution most efficiently by calculating the exchange along
 **  each edge pair, accumulating the net influx/outflux at each node, and
 **  then summing the influx/outflux for each node and dividing by the node's
 **  Voronoi area to get the change in elevation.
 **    To ensure numerical stability, the routine begins by estimating a
 **  maximum stable time-step size for each pair of nodes (ie, each edge)
 **  using the Courant condition Dt <= (Le^2)/(2KdLv). The minimum of these
 **  is used as the maximum step size. If the estimated maximum is smaller
 **  than the "global" time size size (storm+interstorm duration) rt, the
 **  solution is computed in a series of sub-steps until all of rt has been
 **  used up.
 **    The routine includes an option to "turn off" deposition in areas of
 **  concave topography (= net deposition), on the assumption that stream
 **  erosion would quickly remove such material.
 **
 **  Inputs:  rt -- time duration over which to compute diffusion
 **           noDepoFlag -- if true, material is only eroded, never
 **                             deposited
 **  Modifies:  node elevations (z); node Qsin (sed influx) is reset and
 **             used in the computations
 **  Notes:  as of 3/98, does not differentiate between rock and sediment
 **
\*****************************************************************************/
#define kVerySmall 1e-6
#define kEpsOver2 0.1
void tErosion::Diffuse( double rt, int noDepoFlag )
{
   tLNode * cn;
   tEdge * ce;
   double volout,  // Sediment volume output from a node (neg=input)
       denom,      // Denominator in Courant number (=Kd*Lve)
       delt,       // Max local step size
       dtmax;      // Max global step size (initially equal to total time rt)
   tMeshListIter<tLNode> nodIter( meshPtr->getNodeList() );
   tMeshListIter<tEdge> edgIter( meshPtr->getEdgeList() );

#if TRACKFNS
   cout << "tErosion::Diffuse()" << endl << flush;
#endif
   
   // Compute maximum stable time-step size based on Courant condition
   // (Note: for a fixed mesh, this calculation only needs to be done once;
   // performance could be improved by having this block only called if
   // mesh has changed since last time through)
   dtmax = rt;  // Initialize dtmax to total time rt
   for( ce=edgIter.FirstP(); edgIter.IsActive(); ce=edgIter.NextP() )
     {
       if( ce->getVEdgLen() > 5000.0 )
	 {
	   //cout << "In Diffuse(), large vedglen detected: " << ce->getVEdgLen() << endl;
	   //ce->TellCoords();
	   /*cout << "Connected to ORIGIN:\n";
	     ce->getOriginPtr()->TellAll();
	     cout << "DESTINATION:\n";
	     ce->getDestinationPtr()->TellAll();*/
	 }
       if( (denom=kd*ce->getVEdgLen() ) > kVerySmall )
	 {
	   delt = kEpsOver2*(ce->getLength()/denom);
	   if( delt < dtmax )
	     {
	       dtmax = delt;
	       /*cout << "TIME STEP CONSTRAINED TO " << dtmax << " AT EDGE:\n";
		 ce->TellCoords();*/
	     }
	 }
     }
   
   // Loop until we've used up the entire time interval rt
   do
   {
      // Reset sed input for each node for the new iteration
      for( cn=nodIter.FirstP(); nodIter.IsActive(); cn=nodIter.NextP() )
          cn->setQsin( 0 );

      // Compute sediment volume transfer along each edge
      for( ce=edgIter.FirstP(); edgIter.IsActive(); ce=edgIter.NextP() )
      {
         volout = kd*ce->CalcSlope()*ce->getVEdgLen()*dtmax;
         // Record outgoing flux from origin
         cn = (tLNode *)ce->getOriginPtrNC();
         cn->addQsin( -volout );
         // Record incoming flux to dest'n
         cn = (tLNode *)ce->getDestinationPtrNC();
         cn->addQsin( volout );
         /*cout << volout << " mass exch. from " << ce->getOriginPtr()->getID()
              << " to "
              << ce->getDestinationPtr()->getID()
              << " on slp " << ce->getSlope() << " ve " << ce->getVEdgLen()
              << "\nvp " << ce->getRVtx()[0] << " " << ce->getRVtx()[1] << endl;
         ((tLNode*)ce->getOriginPtr())->TellAll();
         ((tLNode*)ce->getDestinationPtr())->TellAll();
         cout << endl;*/
         
         ce = edgIter.NextP();  // Skip complementary edge
      }
   
      // Compute erosion/deposition for each node
      for( cn=nodIter.FirstP(); nodIter.IsActive(); cn=nodIter.NextP() )
      {
             /*cout << "Node " << cn->getID() << " Qsin: " << cn->getQsin() << " dz: " << cn->getQsin() / cn->getVArea() << endl;*/
         if( noDepoFlag )
             if( cn->getQsin() > 0.0 )
                 cn->setQsin( 0.0 );
         cn->EroDep( cn->getQsin() / cn->getVArea() );  // add or subtract net flux/area
         //cout<<cn->getZ()<<" Q: "<<cn->getQ()<<" dz "<<cn->getQsin() / cn->getVArea()<<" dt "<<dtmax<<endl;
         /*if( cn->id==700 ) {
           cn->TellAll();
           }*/
      }

      rt -= dtmax;
      if( dtmax>rt ) dtmax=rt;
      
   } while( rt>0.0 );
   
   
}


/***********************************************************************\
 ** tErosion::UpdateExposureTime( double dtg)                         **
 **                                                                   **
 ** This function increments the exposure time of the top layer at    **
 ** every node by the amount dtg.                                     **
 ** Called from main loop.                                            **
 **                                                                   **
 ** created 3/1999 ng                                                 **
 **********************************************************************/
void tErosion::UpdateExposureTime( double dtg)
{
   tLNode * cn;
   tMeshListIter<tLNode> nodIter( meshPtr->getNodeList() );

#if TRACKFNS
   cout << "tErosion::UpdateExposureTime()" << endl << flush;
#endif
   
   for( cn=nodIter.FirstP(); nodIter.IsActive(); cn=nodIter.NextP() ){
      cn->addLayerEtime(0, dtg);
   }
}


/***********************************************************************\
 **
 ** tErosion::DensifyMesh
 **
 ** Called only when the option for adaptive remeshing is invoked,
 ** this routine is used to increase mesh resolution in areas where
 ** erosion (or deposition) is especially rapid. For each point,
 ** a check is made to see whether the current erosion rate (ie, the
 ** most recently recorded value) times the Voronoi area is greater
 ** than a user-specified threshold, mdMeshAdaptMaxFlux. Note that
 ** the dimensions are L3/T, ie sediment flux being eroded from
 ** (or deposited into) the node; thus the threshold is a maximum
 ** allowable sediment flux resulting from local erosion. If the
 ** flux exceeds this threshold at a given node, new nodes are
 ** added at each of the node's Voronoi vertices.
 **
 **   Created: 2/2000 gt for gully erosion study
 **   Assumptions: assumes node dzdt value is correct
 **
 **********************************************************************/
void tErosion::DensifyMesh( double time )
{
   tMeshListIter<tLNode> niter( meshPtr->getNodeList() );  // node list iter.
   tLNode *cn;              // Current node being checked
   int pointsAdded = FALSE; // Flag: did we do any densifying?
   tLNode * dbgnode;

   double dbgnf, dbgmax=0;
   
   cout << "Checking nodes...\n";
   
   // Check all active nodes
   for( cn=niter.FirstP(); niter.IsActive(); cn=niter.NextP() )
   {
      dbgnf = fabs(cn->getVArea()*cn->getDzDt());
      if( dbgnf>dbgmax ) dbgmax = dbgnf;
      
      // If local flux (ero rate * varea) exceeds threshold, add new nodes
      if( fabs(cn->getVArea()*cn->getDzDt()) > mdMeshAdaptMaxFlux )
      {
         meshPtr->AddNodesAround( cn, time );
         //cout << "*** Adding points here:\n";
         //cn->TellAll();
      }
   }
   
   cout << "Max node flux: " << dbgmax << endl;
   
}

   
         
               
         
