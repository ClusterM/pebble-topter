const CONFIG_HTML = `
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>TOTPer Settings</title>
  <style>
    body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', sans-serif; margin: 0; padding: 16px; background: #121212; color: #f5f5f5; }
    h1 { font-size: 24px; margin: 0 0 20px; text-align: center; }
    .entries { margin-bottom: 20px; }
    .entry { 
      background: #1f1f1f; 
      border-radius: 12px; 
      padding: 16px; 
      margin-bottom: 12px; 
      display: flex; 
      align-items: center; 
      cursor: move; 
      transition: all 0.2s ease;
      border: 2px solid transparent;
      position: relative;
      box-shadow: 0 2px 4px rgba(0,0,0,0.2);
    }
    .entry:hover { 
      background: #252525; 
      box-shadow: 0 4px 8px rgba(0,0,0,0.3);
    }
    .entry.dragging { 
      opacity: 0.4; 
      transform: scale(0.95);
      box-shadow: 0 8px 16px rgba(0,0,0,0.4);
    }
    .entry.drag-over-top::before {
      content: '';
      position: absolute;
      top: -10px;
      left: 0;
      right: 0;
      height: 4px;
      background: #2196F3;
      border-radius: 2px;
      box-shadow: 0 0 10px rgba(33, 150, 243, 0.8);
      z-index: 10;
    }
    .entry.drag-over-bottom::after {
      content: '';
      position: absolute;
      bottom: -10px;
      left: 0;
      right: 0;
      height: 4px;
      background: #2196F3;
      border-radius: 2px;
      box-shadow: 0 0 10px rgba(33, 150, 243, 0.8);
      z-index: 10;
    }
    .entry-content { flex: 1; }
    .entry-label { font-size: 16px; font-weight: bold; margin-bottom: 4px; }
    .entry-account { font-size: 14px; color: #9e9e9e; }
    .entry-remove { 
      background: #f44336; 
      color: white; 
      border: none; 
      border-radius: 50%; 
      width: 32px; 
      height: 32px; 
      cursor: pointer; 
      display: flex; 
      align-items: center; 
      justify-content: center; 
      font-size: 18px;
      transition: all 0.2s ease;
    }
    .entry-remove:hover {
      background: #d32f2f;
      transform: scale(1.1);
    }
    .add-section { background: #2c2c2c; border-radius: 12px; padding: 16px; margin-bottom: 20px; }
    .tabs { display: flex; margin-bottom: 16px; }
    .tab { background: #424242; color: #f5f5f5; border: none; padding: 12px 16px; cursor: pointer; border-radius: 8px 8px 0 0; }
    .tab.active { background: #4caf50; }
    .tab-content { display: none; }
    .tab-content.active { display: block; }
    .form-group { margin-bottom: 12px; }
    .form-group label { display: block; font-size: 14px; margin-bottom: 4px; color: #9e9e9e; }
    .form-group input { width: 100%; padding: 12px; border-radius: 8px; border: none; font-size: 16px; box-sizing: border-box; background: #1f1f1f; color: #f5f5f5; }
    .form-group textarea { width: 100%; padding: 12px; border-radius: 8px; border: none; font-size: 14px; box-sizing: border-box; background: #1f1f1f; color: #f5f5f5; min-height: 80px; font-family: monospace; }
    .qr-section { text-align: center; }
    .qr-result { margin-top: 12px; padding: 12px; background: #1f1f1f; border-radius: 8px; word-break: break-all; font-family: monospace; font-size: 12px; }
    button { border: none; border-radius: 8px; padding: 12px 16px; font-size: 16px; cursor: pointer; margin: 4px; }
    button.primary { background: #4caf50; color: white; }
    button.secondary { background: #424242; color: #f5f5f5; }
    button.danger { background: #f44336; color: white; }
    button:disabled { opacity: 0.45; cursor: not-allowed; }
    .actions { position: sticky; bottom: 0; display: flex; justify-content: space-between; gap: 8px; padding-top: 12px; background: rgba(18,18,18,0.95); margin: -16px; padding: 16px; }
    .hint { font-size: 13px; color: #b0bec5; margin-bottom: 16px; text-align: center; }
    .drag-handle { 
      margin-right: 12px; 
      color: #666; 
      font-size: 18px; 
      cursor: grab;
      transition: color 0.2s ease;
      user-select: none;
    }
    .drag-handle:active {
      cursor: grabbing;
      color: #999;
    }
    .entry:hover .drag-handle {
      color: #999;
    }
  </style>
</head>
<body>
  <h1>TOTPer</h1>
  <div id="entries" class="entries"></div>

  <div class="add-section">
    <div class="tabs">
      <button class="tab active" data-tab="qr">QR Code</button>
      <button class="tab" data-tab="manual">Manual Entry</button>
    </div>

    <div id="qr" class="tab-content active">
      <div class="qr-section">
        <p style="font-weight: bold;">QR Code Import</p>
        <p style="font-size: 12px; color: #b0bec5; line-height: 1.4; margin-bottom: 12px; text-align: left;">
          1. Use any QR scanner app to scan the 2FA QR code<br>
          2. Copy the scanned text (starts with "otpauth://totp/")<br>
          3. Paste it into the field below
        </p>
        <p style="font-size: 12px; color: #9e9e9e;">Multiple URLs supported (one per line).<br/>Duplicates will be skipped.</p>
        <div class="form-group">
          <textarea id="qr-input" placeholder="otpauth://totp/Example:user@example.com?secret=JBSWY3DPEHPK3PXP&issuer=Example" style="min-height: 148px;"></textarea>
        </div>
        <button id="parse-qr" class="primary" style="width: 100%;">Parse & Add Entries</button>
        <div id="qr-result" class="qr-result" style="display: none;"></div>
      </div>
    </div>

    <div id="manual" class="tab-content">
      <div class="form-group">
        <label>Label</label>
        <input type="text" id="manual-label" placeholder="e.g. Google" maxlength="32">
      </div>
      <div class="form-group">
        <label>Account Name (optional)</label>
        <input type="text" id="manual-account" placeholder="e.g. user@gmail.com" maxlength="32">
      </div>
      <div class="form-group">
        <label>Secret (Base32)</label>
        <input type="text" id="manual-secret" placeholder="JBSWY3DPEHPK3PXP" maxlength="64">
      </div>
      <button id="add-manual" class="primary" style="width: 100%;">Add Entry</button>
    </div>
  </div>

  <div class="actions">
    <button id="save" class="primary" style="width: 100%;">Send to the Watch</button>
  </div>

  <script>
  (function() {
    var MAX_ACCOUNTS = 100;
    var initialData = decodeURIComponent('__INITIAL_DATA__');
    var entries = [];
    try {
      if (initialData) {
        var parsed = JSON.parse(initialData);
        if (Array.isArray(parsed)) {
          entries = parsed.slice(0, MAX_ACCOUNTS);
        }
      }
    } catch (err) {
      console.log('Failed to parse initial data', err);
    }

    var entriesRoot = document.getElementById('entries');
    var draggedElement = null;

    function createEntryView(entry, index) {
      var container = document.createElement('div');
      container.className = 'entry';
      container.dataset.index = index;
      container.draggable = true;

      container.innerHTML = [
        '<span class="drag-handle">⋮⋮</span>',
        '<div class="entry-content">',
          '<div class="entry-label">' + (entry.label || 'Unnamed') + '</div>',
          '<div class="entry-account">' + (entry.account_name || '') + '</div>',
        '</div>',
        '<button type="button" class="entry-remove">×</button>'
      ].join('');

      // Remove button
      container.querySelector('.entry-remove').addEventListener('click', function(e) {
        e.stopPropagation();
        if (confirm('Delete this entry?')) {
          entries.splice(index, 1);
          renderEntries();
        }
      });

      // Drag and drop events
      container.addEventListener('dragstart', function(e) {
        draggedElement = container;
        container.classList.add('dragging');
        e.dataTransfer.effectAllowed = 'move';
        e.dataTransfer.setData('text/html', container.innerHTML);
      });

      container.addEventListener('dragend', function(e) {
        container.classList.remove('dragging');
        // Clean up all drag-over classes
        document.querySelectorAll('.entry').forEach(function(el) {
          el.classList.remove('drag-over-top', 'drag-over-bottom');
        });
        draggedElement = null;
      });

      container.addEventListener('dragenter', function(e) {
        e.preventDefault();
        if (draggedElement && draggedElement !== container) {
          // First, clean up all existing indicators
          document.querySelectorAll('.entry').forEach(function(el) {
            el.classList.remove('drag-over-top', 'drag-over-bottom');
          });
          
          // Determine if we should show indicator on top or bottom
          var draggedIndex = parseInt(draggedElement.dataset.index);
          var targetIndex = parseInt(container.dataset.index);
          
          if (draggedIndex < targetIndex) {
            container.classList.add('drag-over-bottom');
          } else {
            container.classList.add('drag-over-top');
          }
        }
      });

      container.addEventListener('dragleave', function(e) {
        // Only remove classes if we're actually leaving the container
        var rect = container.getBoundingClientRect();
        if (e.clientX < rect.left || e.clientX >= rect.right ||
            e.clientY < rect.top || e.clientY >= rect.bottom) {
          container.classList.remove('drag-over-top', 'drag-over-bottom');
        }
      });

      container.addEventListener('dragover', function(e) {
        e.preventDefault();
        e.dataTransfer.dropEffect = 'move';
      });

      container.addEventListener('drop', function(e) {
        e.preventDefault();
        container.classList.remove('drag-over-top', 'drag-over-bottom');
        
        if (draggedElement && draggedElement !== container) {
          var draggedIndex = parseInt(draggedElement.dataset.index);
          var targetIndex = parseInt(container.dataset.index);

          // Reorder entries array
          var draggedEntry = entries.splice(draggedIndex, 1)[0];
          entries.splice(targetIndex, 0, draggedEntry);

          renderEntries();
        }
      });

      return container;
    }

    function renderEntries() {
      entriesRoot.innerHTML = '';
      if (!entries.length) {
        var empty = document.createElement('p');
        empty.className = 'hint';
        empty.textContent = 'No entries yet. Add your first TOTP account below.';
        entriesRoot.appendChild(empty);
      } else {
        var header = document.createElement('p');
        header.className = 'hint';
        header.textContent = 'Accounts: ' + entries.length + ' / ' + MAX_ACCOUNTS;
        entriesRoot.appendChild(header);
        
        entries.forEach(function(entry, idx) {
          entriesRoot.appendChild(createEntryView(entry, idx));
        });
      }
    }

    function buildPayload(list) {
      return list.map(function(item) {
        var algo = item.algorithm !== undefined ? item.algorithm : 0;
        return [item.label, item.account_name, item.secret, item.period, item.digits, algo].join('|');
      }).join(';');
    }

    // Base32 decode helper
    function base32ToHex(base32) {
      var base32chars = 'ABCDEFGHIJKLMNOPQRSTUVWXYZ234567';
      var bits = '';
      var hex = '';
      
      base32 = base32.replace(/=+$/, '');
      
      for (var i = 0; i < base32.length; i++) {
        var val = base32chars.indexOf(base32.charAt(i).toUpperCase());
        if (val === -1) continue;
        bits += val.toString(2).padStart(5, '0');
      }
      
      for (var i = 0; i + 8 <= bits.length; i += 8) {
        var chunk = bits.substr(i, 8);
        hex += String.fromCharCode(parseInt(chunk, 2));
      }
      
      return hex;
    }

    // Parse Google Authenticator migration format
    function parseGoogleAuthMigration(text) {
      try {
        // Extract data parameter
        var match = text.match(/otpauth-migration:\\/\\/offline\\?data=([^&\\s]+)/);
        if (!match) {
          throw new Error('Invalid migration URL');
        }
        
        var data = decodeURIComponent(match[1]);
        var bytes = atob(data);
        
        var accounts = [];
        var pos = 0;
        
        // Simple protobuf parser for Google Authenticator format
        while (pos < bytes.length) {
          var tag = bytes.charCodeAt(pos++);
          var wireType = tag & 0x07;
          var fieldNumber = tag >> 3;
          
          if (wireType === 2) { // Length-delimited
            var length = bytes.charCodeAt(pos++);
            var value = bytes.substr(pos, length);
            pos += length;
            
            if (fieldNumber === 1) { // otp_parameters
              var account = parseOtpParameters(value);
              if (account) {
                accounts.push(account);
              }
            }
          } else {
            // Skip other types
            pos++;
          }
        }
        
        return accounts;
      } catch (err) {
        throw new Error('Failed to parse Google Auth migration: ' + err.message);
      }
    }
    
    function parseOtpParameters(data) {
      var secret = '';
      var name = '';
      var issuer = '';
      var algorithm = 0;
      var digits = 6;
      var type = 2; // TOTP
      
      var pos = 0;
      while (pos < data.length) {
        var tag = data.charCodeAt(pos++);
        var wireType = tag & 0x07;
        var fieldNumber = tag >> 3;
        
        if (wireType === 2) { // Length-delimited (bytes/string)
          var length = data.charCodeAt(pos++);
          var value = data.substr(pos, length);
          pos += length;
          
          if (fieldNumber === 1) { // secret
            // Convert bytes to base32
            var hex = '';
            for (var i = 0; i < value.length; i++) {
              hex += value.charCodeAt(i).toString(16).padStart(2, '0');
            }
            secret = hexToBase32(hex);
          } else if (fieldNumber === 2) { // name
            name = value;
          } else if (fieldNumber === 3) { // issuer
            issuer = value;
          }
        } else if (wireType === 0) { // Varint
          var value = data.charCodeAt(pos++);
          
          if (fieldNumber === 4) { // algorithm
            algorithm = value - 1; // Convert from proto enum (1=SHA1, 2=SHA256, 3=SHA512)
            if (algorithm < 0) algorithm = 0;
          } else if (fieldNumber === 5) { // digits
            digits = value === 2 ? 8 : 6;
          } else if (fieldNumber === 6) { // type
            type = value;
          }
        } else {
          pos++;
        }
      }
      
      if (!secret || type !== 2) { // Only TOTP
        return null;
      }
      
      // Parse name to extract account if needed
      var account_name = '';
      if (name.includes(':')) {
        var parts = name.split(':');
        if (!issuer) issuer = parts[0];
        account_name = parts.slice(1).join(':');
      } else {
        account_name = name;
      }
      
      return {
        label: issuer || 'Unknown',
        account_name: account_name,
        secret: secret,
        period: 30,
        digits: digits,
        algorithm: algorithm
      };
    }
    
    function hexToBase32(hex) {
      var base32chars = 'ABCDEFGHIJKLMNOPQRSTUVWXYZ234567';
      var bits = '';
      
      for (var i = 0; i < hex.length; i += 2) {
        var byte = parseInt(hex.substr(i, 2), 16);
        bits += byte.toString(2).padStart(8, '0');
      }
      
      var base32 = '';
      for (var i = 0; i + 5 <= bits.length; i += 5) {
        var chunk = bits.substr(i, 5);
        base32 += base32chars[parseInt(chunk, 2)];
      }
      
      return base32;
    }

    function parseOtpUri(text) {
      try {
        var url = new URL(text);
        if (url.protocol !== 'otpauth:') {
          throw new Error('Not an otpauth URL');
        }

        var type = url.hostname;
        if (type !== 'totp') {
          throw new Error('Only TOTP is supported');
        }

        var params = {};
        url.searchParams.forEach(function(value, key) {
          params[key.toLowerCase()] = value;
        });

        var label = decodeURIComponent(url.pathname.substring(1) || '');
        var issuer = params.issuer || '';

        // Parse label (format: issuer:account or just account)
        var account = '';
        if (label.includes(':')) {
          var parts = label.split(':');
          if (!issuer) issuer = parts[0];
          account = parts.slice(1).join(':');
        } else {
          account = label;
        }

        var secret = (params.secret || '').toUpperCase().replace(/[^A-Z2-7]/g, '');
        var digits = parseInt(params.digits, 10) || 6;
        var period = parseInt(params.period, 10) || 30;
        
        // Parse algorithm (SHA1, SHA256, SHA512)
        var algorithmStr = (params.algorithm || 'SHA1').toUpperCase();
        var algorithm = 0;  // Default to SHA1
        if (algorithmStr === 'SHA256') {
          algorithm = 1;
        } else if (algorithmStr === 'SHA512') {
          algorithm = 2;
        }

        if (!secret) {
          throw new Error('No secret found');
        }
        if (digits < 6 || digits > 8) {
          digits = 6;
        }
        if (period < 1 || period > 120) {
          period = 30;
        }

        return {
          label: issuer,
          account_name: account,
          secret: secret,
          period: period,
          digits: digits,
          algorithm: algorithm
        };
      } catch (err) {
        throw new Error('Invalid otpauth URL: ' + err.message);
      }
    }

    function isDuplicate(newEntry) {
      return entries.some(function(existingEntry) {
        return existingEntry.secret === newEntry.secret;
      });
    }

    function processMultipleUrls(text) {
      var newline = String.fromCharCode(10);
      var lines = text.split(newline);
      var results = { added: 0, skipped: 0, errors: 0, limitReached: false };
      
      for (var i = 0; i < lines.length; i++) {
        var line = lines[i].trim();
        if (!line) continue;
        
        // Check if it's a Google Authenticator migration URL
        if (line.startsWith('otpauth-migration://')) {
          try {
            var migratedAccounts = parseGoogleAuthMigration(line);
            for (var j = 0; j < migratedAccounts.length; j++) {
              // Check limit before adding
              if (entries.length >= MAX_ACCOUNTS) {
                results.limitReached = true;
                break;
              }
              
              var entry = migratedAccounts[j];
              if (isDuplicate(entry)) {
                results.skipped++;
              } else {
                entries.push(entry);
                results.added++;
              }
            }
            if (results.limitReached) break;
          } catch (e) {
            results.errors++;
          }
          continue;
        }
        
        // Regular otpauth:// URL
        // Check limit before processing
        if (entries.length >= MAX_ACCOUNTS) {
          results.limitReached = true;
          break;
        }
        
        try {
          var entry = parseOtpUri(line);
          if (isDuplicate(entry)) {
            results.skipped++;
          } else {
            entries.push(entry);
            results.added++;
          }
        } catch (e) {
          results.errors++;
        }
      }
      
      return results;
    }

    function hideQrResult() {
      var resultDiv = document.getElementById('qr-result');
      if (resultDiv) {
        resultDiv.style.display = 'none';
      }
    }

    function switchTab(tabName) {
      // Hide all tabs
      var tabs = document.querySelectorAll('.tab');
      var contents = document.querySelectorAll('.tab-content');

      tabs.forEach(function(tab) {
        tab.classList.remove('active');
      });
      contents.forEach(function(content) {
        content.classList.remove('active');
      });

      // Show selected tab
      document.querySelector('[data-tab="' + tabName + '"]').classList.add('active');
      document.getElementById(tabName).classList.add('active');
    }

    function addManualEntry() {
      if (entries.length >= MAX_ACCOUNTS) {
        alert('Maximum limit of ' + MAX_ACCOUNTS + ' accounts reached!');
        return;
      }

      var label = document.getElementById('manual-label').value.trim();
      var account = document.getElementById('manual-account').value.trim();
      var secret = document.getElementById('manual-secret').value.trim().toUpperCase().replace(/[^A-Z2-7]/g, '');

      if (!label || !secret) {
        alert('Label and Secret are required!');
        return;
      }

      var newEntry = {
        label: label,
        account_name: account,
        secret: secret,
        period: 30,
        digits: 6,
        algorithm: 0  // Default to SHA1
      };

      if (isDuplicate(newEntry)) {
        alert('This secret already exists in your list!');
        return;
      }

      entries.push(newEntry);

      // Clear form
      document.getElementById('manual-label').value = '';
      document.getElementById('manual-account').value = '';
      document.getElementById('manual-secret').value = '';

      renderEntries();
    }

    function addQrEntry() {
      var qrText = document.getElementById('qr-input').value.trim();
      var resultDiv = document.getElementById('qr-result');

      if (!qrText) {
        alert('Please paste at least one otpauth URL!');
        return;
      }

      var results = processMultipleUrls(qrText);
      
      if (results.added === 0 && results.skipped === 0 && results.errors === 0) {
        alert('No valid URLs found!');
        return;
      }

      var resultText = '';
      if (results.added > 0) {
        if (results.added === 1) {
          resultText += 'Entry successfully added';
        } else {
          resultText += results.added + ' entries successfully added';
        }
      }
      if (results.skipped > 0) {
        if (resultText) resultText += '<br>';
        if (results.skipped === 1) {
          resultText += '1 duplicate skipped';
        } else {
          resultText += results.skipped + ' duplicates skipped';
        }
      }
      if (results.errors > 0) {
        if (resultText) resultText += '<br>';
        if (results.errors === 1) {
          resultText += 'Parse error: 1 URL failed';
        } else {
          resultText += 'Parse errors: ' + results.errors + ' URLs failed';
        }
      }
      if (results.limitReached) {
        if (resultText) resultText += '<br>';
        resultText += '<strong>Limit reached: ' + MAX_ACCOUNTS + ' accounts maximum</strong>';
      }

      resultDiv.style.display = 'block';
      resultDiv.innerHTML = resultText;
      
      setTimeout(hideQrResult, 5000);

      if (results.added > 0) {
        document.getElementById('qr-input').value = '';
        renderEntries();
      }
    }

    // Tab switching
    document.querySelectorAll('.tab').forEach(function(tab) {
      tab.addEventListener('click', function() {
        switchTab(this.dataset.tab);
      });
    });

    // Manual entry
    document.getElementById('add-manual').addEventListener('click', addManualEntry);

    // QR entry
    document.getElementById('parse-qr').addEventListener('click', addQrEntry);

    // Save button
    document.getElementById('save').addEventListener('click', function() {
      if (!entries.length) {
        alert('Add at least one entry first.');
        return;
      }
      var payload = buildPayload(entries);
      var result = {
        entries: entries,
        payload: payload
      };
      var url = 'pebblejs://close#' + encodeURIComponent(JSON.stringify(result));
      window.location = url;
    });

    renderEntries();
  })();
  </script>
</body>
</html>
`;

function buildConfigUrl(initialEntries) {
  const initialEncoded = encodeURIComponent(initialEntries || '');
  const htmlWithData = CONFIG_HTML.replace('__INITIAL_DATA__', initialEncoded);
  return 'data:text/html;charset=utf-8,' + encodeURIComponent(htmlWithData);
}

function sendPayloadToWatch(payload) {
  return new Promise((resolve, reject) => {
    const entries = payload.split(';').filter(entry => entry.trim() !== '');

    Pebble.sendAppMessage(
      { AppKeyCount: entries.length },
      () => {
        const totalCount = entries.length;

        if (totalCount === 0) {
          resolve();
          return;
        }

        function sendNextEntry(index) {
          if (index >= totalCount) {
            resolve();
            return;
          }

          Pebble.sendAppMessage(
            {
              AppKeyEntryId: index,
              AppKeyEntry: entries[index].trim()
            },
            () => {
              setTimeout(() => sendNextEntry(index + 1), 100);
            },
            err => {
              console.log('Failed to send entry', index, ':', err);
              reject(err);
            }
          );
        }

        setTimeout(() => sendNextEntry(0), 200);
      },
      err => reject(err)
    );
  });
}

function requestResend() {
  Pebble.sendAppMessage({ AppKeyRequest: 1 }, () => {}, () => {});
}

Pebble.addEventListener('ready', () => {
  requestResend();
});

Pebble.addEventListener('showConfiguration', () => {
  const storedEntries = localStorage.getItem('TOTPerConfigEntries') || '';
  const url = buildConfigUrl(storedEntries);
  Pebble.openURL(url);
});

Pebble.addEventListener('webviewclosed', e => {
  if (!e || !e.response) {
    return;
  }
  let data;
  try {
    data = JSON.parse(decodeURIComponent(e.response));
  } catch (err) {
    console.log('Failed to parse config response', err);
    return;
  }

  if (data.reset) {
    localStorage.removeItem('TOTPerConfigEntries');
    localStorage.removeItem('TOTPerConfigPayload');
    return;
  }

  if (!Array.isArray(data.entries) || typeof data.payload !== 'string') {
    return;
  }

  localStorage.setItem('TOTPerConfigEntries', JSON.stringify(data.entries));
  localStorage.setItem('TOTPerConfigPayload', data.payload);
  sendPayloadToWatch(data.payload).catch(err => {
    console.log('Failed to send payload', err);
  });
});

Pebble.addEventListener('appmessage', () => {
  // Message handling not needed for unidirectional sync
});

