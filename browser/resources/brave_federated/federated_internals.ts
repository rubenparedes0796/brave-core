import {addWebUIListener} from 'chrome://resources/js/cr.m.js';
import {decorate} from 'chrome://resources/js/cr/ui.m.js';
import {TabBox} from 'chrome://resources/js/cr/ui/tabs.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';

import {$} from 'chrome://resources/js/util.m.js';

import {AdStoreLog} from './segmentation_internals.mojom-webui.js';
import {FederatedInternalsBrowserProxy} from './segmentation_internals_browser_proxy.js';

function getProxy(): FederatedInternalsBrowserProxy {
  return FederatedInternalsBrowserProxy.getInstance();
}

const dataStoresLogs = {};
let selectedDataStore = "ad-timing";

function initialize() {
  decorate('tabbox', TabBox);

  getProxy().getAdStoreInfo();
  
  getProxy().getCallbackRouter().onAdsTimingDataStoreLogsLoaded.addListener(
    (ad_store_logs: Array<AdStoreLog>) => {
      dataStoresLogs['ad-timing'] = logs;
      onDataStoreChanged();
  });

  const button = $('data-store-logs-dump');
  button.addEventListener('click', onLogsDump);

  const tabpanelNodeList = document.getElementsByTagName('tabpanel');
  const tabpanels = Array.prototype.slice.call(tabpanelNodeList, 0);
  const tabpanelIds = tabpanels.map(function(tab) {
    return tab.id;
  });

  const tabNodeList = document.getElementsByTagName('tab');
  const tabs = Array.prototype.slice.call(tabNodeList, 0);
  tabs.forEach(function(tab) {
    tab.onclick = function(e) {
      const tabbox = document.querySelector('tabbox');
      const tabpanel = tabpanels[tabbox.selectedIndex];
      const hash = tabpanel.id.match(/(?:^tabpanel-)(.+)/)[1];
      window.location.hash = hash;
    };
  });

  const activateTabByHash = function() {
    let hash = window.location.hash;

    // Remove the first character '#'.
    hash = hash.substring(1);

    const id = 'tabpanel-' + hash;
    if (tabpanelIds.indexOf(id) === -1) {
      return;
    }

    $(id).selected = true;
  };

  window.onhashchange = activateTabByHash;
  $('stores').onchange = onDataStoreChanged;
  activateTabByHash();
}

function onDataStoreChanged() {
    selectedDataStore = $('stores').value;
    const logs = dataStoresLogs[selectedDataStore]; 

    if (selectedDataStore == 'ad-timing') {
        Object.keys(logs[0]).forEach(function(title) {
            const th = document.createElement('th');
            th.textContent = title;
            th.className = 'ad-timing-log-'+ title;
        
            const thead = $('ad-timing-headers');
            thead.appendChild(th);
        });
    
      logs.forEach(function(log) {
        const tr = document.createElement('tr');
        appendTD(tr, log.log_id, 'ad-timing-log-id');
        appendTD(tr, formatDate(new Date(log.log_time)), 'ad-timing-log-time');
        appendTD(tr, log.log_locale, 'ad-timing-log-locale');
        appendTD(tr, log.log_number_of_tabs, 'ad-timing-log-number_of_tabs');
        appendBooleanTD(tr, log.log_label, 'ad-timing-log-label');
    
        const tabpanel = $('tabpanel-data-store-logs');
        const tbody = tabpanel.getElementsByTagName('tbody')[0];
        tbody.appendChild(tr);
      });
    }
}

// COSMETICS

 function appendTD(parent, content, className) {
  const td = document.createElement('td');
  td.textContent = content;
  td.className = className;
  parent.appendChild(td);
}

function appendBooleanTD(parent, value, className) {
  const td = document.createElement('td');
  td.textContent = value;
  td.className = className;
  td.bgColor = value ? '#3cba54' : '#db3236';
  parent.appendChild(td);
}

function padWithZeros(number, width) {
  const numberStr = number.toString();
  const restWidth = width - numberStr.length;
  if (restWidth <= 0) {
    return numberStr;
  }

  return Array(restWidth + 1).join('0') + numberStr;
}

function formatDate(date) {
  const year = date.getFullYear();
  const month = date.getMonth() + 1;
  const day = date.getDate();
  const hour = date.getHours();
  const minute = date.getMinutes();
  const second = date.getSeconds();

  const yearStr = padWithZeros(year, 4);
  const monthStr = padWithZeros(month, 2);
  const dayStr = padWithZeros(day, 2);
  const hourStr = padWithZeros(hour, 2);
  const minuteStr = padWithZeros(minute, 2);
  const secondStr = padWithZeros(second, 2);

  const str = yearStr + '-' + monthStr + '-' + dayStr + ' ' + hourStr + ':' +
      minuteStr + ':' + secondStr;

  return str;
}

function onLogsDump() {
    const data = JSON.stringify(dataStoresLogs[selectedDataStore]);
    const blob = new Blob([data], {'type': 'text/json'});
    const url = URL.createObjectURL(blob);
    const filename = 'data_store_dump.json'; // TODO: Add dumped store name
  
    const a = document.createElement('a');
    a.setAttribute('href', url);
    a.setAttribute('download', filename);
  
    const event = document.createEvent('MouseEvent');
    event.initMouseEvent(
        'click', true, true, window, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, null);
    a.dispatchEvent(event);
  }

document.addEventListener('DOMContentLoaded', initialize);
