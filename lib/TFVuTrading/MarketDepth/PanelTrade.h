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
 * File:    PanelTrade.h
 * Author:  raymond@burkholder.net
 * Project: TFVuTrading/MarketDepth
 * Created on October 28, 2021, 15:55
 */

#pragma once

#include <vector>

#include <wx/panel.h>

#include "RowElements.h"

namespace ou { // One Unified
namespace tf { // TradeFrame
namespace l2 { // market depth

#define SYMBOL_PANELTRADE_STYLE wxTAB_TRAVERSAL
#define SYMBOL_PANELTRADE_TITLE _("Trade Market Depth")
#define SYMBOL_PANELTRADE_IDNAME ID_PANELTRADE
#define SYMBOL_PANELTRADE_SIZE wxSize(400, 300)
#define SYMBOL_PANELTRADE_POSITION wxDefaultPosition

class PanelTrade: public wxPanel {
public:

  PanelTrade();
  PanelTrade(
   wxWindow* parent,
   wxWindowID id = SYMBOL_PANELTRADE_IDNAME,
   const wxPoint& pos = SYMBOL_PANELTRADE_POSITION,
   const wxSize& size = SYMBOL_PANELTRADE_SIZE,
   long style = SYMBOL_PANELTRADE_STYLE
   );
  ~PanelTrade(void);

  bool Create(
   wxWindow* parent,
   wxWindowID id = SYMBOL_PANELTRADE_IDNAME,
   const wxPoint& pos = SYMBOL_PANELTRADE_POSITION,
   const wxSize& size = SYMBOL_PANELTRADE_SIZE,
   long style = SYMBOL_PANELTRADE_STYLE
   );

protected:
private:

  enum {
    ID_Null=wxID_HIGHEST, ID_PANELTRADE
  };

  static const unsigned int FontHeight = 15;
  static const unsigned int RowHeight = 20;
  static const unsigned int BorderWidth = 5;
  static const unsigned int FramedRows = 10; // when to move into frame then recenter

  unsigned int m_nRowCount;
  unsigned int m_nVisibleRows;
  unsigned int m_nFramedRows;
  unsigned int m_nCenteredRows;

  using vRowElements_t = std::vector<RowElements*>;
  vRowElements_t m_vRowElements;

  void Init( void );
  void CreateControls( void );
  bool ShowToolTips( void ) { return true; };

  void DrawRows();

};

} // market depth
} // namespace tf
} // namespace ou