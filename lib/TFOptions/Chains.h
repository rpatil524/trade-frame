/************************************************************************
 * Copyright(c) 2021, One Unified. All rights reserved.                 *
 * email: info@oneunified.net                                           *
 *                                                                      *
 * This file is provided as is WITHOUT ANY WARRANTY                     *
 *  without even the implied warranty of                                *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                *
 *                                                                      *
 * This software may not be used nor distributed without proper license *
 * agreement.                                                           *
 *                                                                      *
 * See the file LICENSE.txt for redistribution information.             *
 ************************************************************************/

/*
 * File:    Chains.h
 * Author:  raymond@burkholder.net
 * Project: TFOptions
 * Created on May 29, 2021, 21:51
 */

#pragma once

#include <string>

#include <boost/log/trivial.hpp>

#include <boost/date_time/gregorian/greg_date.hpp>

#include "Exceptions.h"
#include "GatherOptions.h"

namespace ou { // One Unified
namespace tf { // TradeFrame
namespace option { // options

// for reference:
//    using fGatherOptions_t = std::function<void(const std::string& sUnderlying, fOption_t&&)>;

using pInstrument_t = ou::tf::Instrument::pInstrument_t;

// called by PopulateMap below
template<typename mapChains_t>
typename mapChains_t::iterator GetChain( mapChains_t& map, pInstrument_t& pOptionInstrument ) { // find existing expiry, or create new one

  using chain_t = typename mapChains_t::mapped_type;
  using iterator_t = typename mapChains_t::iterator;

  const boost::gregorian::date expiry( pOptionInstrument->GetExpiry() );

  iterator_t iterChains = map.find( expiry ); // see if expiry date exists
  if ( map.end() == iterChains ) { // insert new expiry set if not
    const std::string& sInstrumentName( pOptionInstrument->GetInstrumentName() );
    const std::string& sIQFeedSymbolName( pOptionInstrument->GetInstrumentName( ou::tf::Instrument::eidProvider_t::EProviderIQF ) );
    BOOST_LOG_TRIVIAL(info)
      << "GetChain created: "
      << ou::tf::Instrument::BuildDate( expiry )
      << ',' << sIQFeedSymbolName
      << ',' << sInstrumentName
      ;
    chain_t chain; // default chain for insertion into new entry
    auto result = map.emplace( expiry, std::move( chain ) );
    assert( result.second );
    iterChains = result.first;
  }
  return iterChains;
}

// called by PopulateMap below
template<typename chain_t, typename OptionEntry>
OptionEntry* UpdateOption( chain_t& chain, pInstrument_t& pOptionInstrument ) {

  // populate new call or put, no test for pre-existance
  //std::cout << "  option: " << row.sSymbol << std::endl;

  OptionEntry* pOptionEntry( nullptr );  // the put/call object at the strike
  const std::string& sIQFeedSymbolName( pOptionInstrument->GetInstrumentName( ou::tf::Instrument::eidProvider_t::EProviderIQF ) );

  try {
    switch ( pOptionInstrument->GetOptionSide() ) {
      case ou::tf::OptionSide::Call:
        {
          OptionEntry& entry( chain.SetIQFeedNameCall( pOptionInstrument->GetStrike(), sIQFeedSymbolName ) );
          pOptionEntry = &entry;
        }
        break;
      case ou::tf::OptionSide::Put:
        {
          OptionEntry& entry( chain.SetIQFeedNamePut( pOptionInstrument->GetStrike(), sIQFeedSymbolName ) );
          pOptionEntry = &entry;
        }
        break;
      default:
        assert( false );
        break;
    }
  }
  catch ( std::runtime_error& e ) {
    BOOST_LOG_TRIVIAL(error) << "option::UpdateOption error";
  }
  return pOptionEntry;
}

template<typename mapChains_t> // populate option constructs into a chain
void PopulateMap( mapChains_t& mapChains, pInstrument_t pUnderlying, fGatherOptions_t&& fGatherOptions ) {
  fGatherOptions(
    pUnderlying,
    [&mapChains](pOption_t pOption){

      using chain_t = typename mapChains_t::mapped_type;
      using iterator_t = typename mapChains_t::iterator;
      using OptionEntry = typename chain_t::option_t;

      pInstrument_t pInstrument( pOption->GetInstrument() );

      iterator_t iterChains = GetChain( mapChains, pInstrument );
      assert( mapChains.end() != iterChains );
      chain_t& chain( iterChains->second );
      UpdateOption<chain_t,OptionEntry>( chain, pInstrument );
  });
}

template<typename mapChains_t>
static typename mapChains_t::const_iterator SelectChain( const mapChains_t& mapChains, boost::gregorian::date date, boost::gregorian::days daysToExpiry ) {
  typename mapChains_t::const_iterator citerChain = std::find_if( mapChains.begin(), mapChains.end(),
    [date,daysToExpiry](const typename mapChains_t::value_type& vt)->bool{
      return daysToExpiry <= ( vt.first - date );  // first chain where trading date less than expiry date
  } );
  if ( mapChains.end() == citerChain ) {
    throw ou::tf::option::exception_chain_not_found( "option::SelectChain" );
  }
  return citerChain;
}

template<typename mapChains_t>
static typename mapChains_t::iterator SelectChain( mapChains_t& mapChains, boost::gregorian::date date, boost::gregorian::days daysToExpiry ) {
  typename mapChains_t::iterator citerChain = std::find_if( mapChains.begin(), mapChains.end(),
    [date,daysToExpiry](const typename mapChains_t::value_type& vt)->bool{
      return daysToExpiry <= ( vt.first - date );  // first chain where trading date less than expiry date
  } );
  if ( mapChains.end() == citerChain ) {
    throw ou::tf::option::exception_chain_not_found( "option::SelectChain" );
  }
  return citerChain;
}

} // namespace option
} // namespace tf
} // namespace ou
