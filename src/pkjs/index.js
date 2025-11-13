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
    .entry { background: #1f1f1f; border-radius: 12px; padding: 16px; margin-bottom: 12px; display: flex; align-items: center; cursor: move; }
    .entry.dragging { opacity: 0.5; }
    .entry-content { flex: 1; }
    .entry-label { font-size: 16px; font-weight: bold; margin-bottom: 4px; }
    .entry-account { font-size: 14px; color: #9e9e9e; }
    .entry-remove { background: #f44336; color: white; border: none; border-radius: 50%; width: 32px; height: 32px; cursor: pointer; display: flex; align-items: center; justify-content: center; font-size: 18px; }
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
    .drag-handle { margin-right: 12px; color: #666; font-size: 18px; }
  </style>
</head>
<body>
  <h1>TOTPer</h1>
  <div id="entries" class="entries"></div>

  <div class="add-section">
    <div class="tabs">
      <button class="tab active" data-tab="manual">Manual Entry</button>
      <button class="tab" data-tab="qr">QR Code</button>
    </div>

    <div id="manual" class="tab-content active">
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

    <div id="qr" class="tab-content">
      <div class="qr-section">
        <p>Paste otpauth URL(s) from QR code(s):</p>
        <p style="font-size: 12px; color: #9e9e9e;">One URL per line. Duplicates will be skipped.</p>
        <div class="form-group">
          <textarea id="qr-input" placeholder="otpauth://totp/..." style="min-height: 148px;"></textarea>
        </div>
        <button id="parse-qr" class="primary" style="width: 100%;">Parse & Add Entries</button>
        <div id="qr-result" class="qr-result" style="display: none;"></div>
      </div>
    </div>
  </div>

  <div class="actions">
    <button id="save" class="primary" style="width: 100%;">Send to Watch</button>
  </div>

  <script>
  (function() {
    console.log('[TOTPer] Script started');
    var initialData = decodeURIComponent('__INITIAL_DATA__');
    var entries = [];
    console.log('[TOTPer] Initial data length:', initialData ? initialData.length : 0);
    try {
      if (initialData) {
        var parsed = JSON.parse(initialData);
        if (Array.isArray(parsed)) {
          entries = parsed;
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
      });

      container.addEventListener('dragend', function(e) {
        container.classList.remove('dragging');
        draggedElement = null;
      });

      container.addEventListener('dragover', function(e) {
        e.preventDefault();
        e.dataTransfer.dropEffect = 'move';
      });

      container.addEventListener('drop', function(e) {
        e.preventDefault();
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
        entries.forEach(function(entry, idx) {
          entriesRoot.appendChild(createEntryView(entry, idx));
        });
      }
    }

    function buildPayload(list) {
      return list.map(function(item) {
        return [item.label, item.account_name, item.secret, item.period, item.digits].join('|');
      }).join(';');
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
          digits: digits
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
      var results = { added: 0, skipped: 0, errors: 0 };
      
      for (var i = 0; i < lines.length; i++) {
        var line = lines[i].trim();
        if (!line) continue;
        
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
        digits: 6
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
      console.log('[TOTPer] Save button clicked');
      console.log('[TOTPer] Current entries:', entries);
      if (!entries.length) {
        alert('Add at least one entry first.');
        return;
      }
      var payload = buildPayload(entries);
      console.log('[TOTPer] Built payload:', payload);
      var result = {
        entries: entries,
        payload: payload
      };
      var url = 'pebblejs://close#' + encodeURIComponent(JSON.stringify(result));
      console.log('[TOTPer] Closing with URL:', url);
      window.location = url;
    });

    renderEntries();
    console.log('[TOTPer] Initialization complete');
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
  console.log('[TOTPer] sendPayloadToWatch called with payload:', payload);
  return new Promise((resolve, reject) => {
    // Parse payload into individual entries
    const entries = payload.split(';').filter(entry => entry.trim() !== '');
    console.log('[TOTPer] Parsed entries:', entries);

    // First send the count of entries
    console.log('[TOTPer] Sending count:', entries.length);
    Pebble.sendAppMessage(
      { AppKeyCount: entries.length },
      () => {
        console.log('[TOTPer] Count sent successfully');

        // Then send each entry individually with delay
        let sentCount = 0;
        const totalCount = entries.length;

        if (totalCount === 0) {
          resolve();
          return;
        }

        function sendNextEntry(index) {
          if (index >= totalCount) {
            console.log('[TOTPer] All entries sent, resolving');
            resolve();
            return;
          }

          console.log('[TOTPer] Sending entry', index, ':', entries[index].trim());
          Pebble.sendAppMessage(
            {
              AppKeyEntryId: index,
              AppKeyEntry: entries[index].trim()
            },
            () => {
              console.log('[TOTPer] Entry', index, 'sent successfully');
              sentCount++;
              // Send next one after 100ms
              setTimeout(() => sendNextEntry(index + 1), 100);
            },
            err => {
              console.log('[TOTPer] Failed to send entry', index, ':', err);
              reject(err);
            }
          );
        }

        // Start sending first entry 200ms after count
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
  console.log('TOTPer ready');
  // Don't send stored data automatically on startup
  // They will be sent only on explicit save
  requestResend();
});

Pebble.addEventListener('showConfiguration', () => {
  console.log('[TOTPer] showConfiguration called');
  const storedEntries = localStorage.getItem('TOTPerConfigEntries') || '';
  console.log('[TOTPer] storedEntries:', storedEntries);
  const url = buildConfigUrl(storedEntries);
  console.log('[TOTPer] Opening URL:', url);
  Pebble.openURL(url);
});

Pebble.addEventListener('webviewclosed', e => {
  console.log('[TOTPer] webviewclosed called', e);
  if (!e || !e.response) {
    console.log('[TOTPer] No response in webviewclosed');
    return;
  }
  let data;
  try {
    data = JSON.parse(decodeURIComponent(e.response));
    console.log('[TOTPer] Parsed data:', data);
  } catch (err) {
    console.log('Failed to parse config response', err);
    return;
  }

  if (data.reset) {
    localStorage.removeItem('TOTPerConfigEntries');
    localStorage.removeItem('TOTPerConfigPayload');
    // No need to send clear command to watch - just clear local storage
    return;
  }

  if (!Array.isArray(data.entries) || typeof data.payload !== 'string') {
    console.log('Config response invalid');
    return;
  }

  localStorage.setItem('TOTPerConfigEntries', JSON.stringify(data.entries));
  localStorage.setItem('TOTPerConfigPayload', data.payload);
  sendPayloadToWatch(data.payload).catch(err => {
    console.log('Failed to send payload', err);
  });
});

Pebble.addEventListener('appmessage', e => {
  console.log('[TOTPer] Received appmessage:', e);
  const payload = e.payload || {};
  if (payload.AppKeyRequest) {
    console.log('[TOTPer] Received AppKeyRequest - ignoring for unidirectional sync');
    // For unidirectional sync, don't send data automatically
  }
  if (payload.AppKeyStatus !== undefined) {
    console.log('[TOTPer] Watch status:', payload.AppKeyStatus);
  }
});

