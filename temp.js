  (function() {
    console.log('[TOTPer] Test interface started');
    var entries = [];
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

      container.querySelector('.entry-remove').addEventListener('click', function(e) {
        e.stopPropagation();
        entries.splice(index, 1);
        renderEntries();
      });

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

          var draggedEntry = entries.splice(draggedIndex, 1)[0];
          entries.splice(targetIndex, 0, draggedEntry);

          renderEntries();
        }
      });

      return container;
    }

    function renderEntries() {
      var entriesRoot = document.getElementById('entries');
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

    function switchTab(tabName) {
      var tabs = document.querySelectorAll('.tab');
      var contents = document.querySelectorAll('.tab-content');

      tabs.forEach(function(tab) {
        tab.classList.remove('active');
      });
      contents.forEach(function(content) {
        content.classList.remove('active');
      });

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

      entries.push({
        label: label,
        account_name: account,
        secret: secret,
        period: 30,
        digits: 6
      });

      document.getElementById('manual-label').value = '';
      document.getElementById('manual-account').value = '';
      document.getElementById('manual-secret').value = '';

      renderEntries();
    }

    function addQrEntry() {
      var qrText = document.getElementById('qr-input').value.trim();
      var resultDiv = document.getElementById('qr-result');

      if (!qrText) {
        alert('Please paste an otpauth URL!');
        return;
      }

      try {
        var entry = parseOtpUri(qrText);
        entries.push(entry);

        resultDiv.style.display = 'block';
        resultDiv.innerHTML = '✓ Successfully added:<br>' +
          '<strong>' + entry.label + '</strong><br>' +
          (entry.account_name ? entry.account_name + '<br>' : '') +
          'Secret: ' + entry.secret.substring(0, 8) + '...<br>' +
          'Period: ' + entry.period + 's, Digits: ' + entry.digits;

        document.getElementById('qr-input').value = '';
        renderEntries();
      } catch (err) {
        resultDiv.style.display = 'block';
        resultDiv.innerHTML = '✗ Error: ' + err.message;
      }
    }

    // Tab switching
    document.querySelectorAll('.tab').forEach(function(tab) {
      tab.addEventListener('click', function() {
        switchTab(this.dataset.tab);
      });
    });

    document.getElementById('add-manual').addEventListener('click', addManualEntry);
    document.getElementById('parse-qr').addEventListener('click', addQrEntry);

    document.getElementById('save').addEventListener('click', function() {
      console.log('[TOTPer] Save button clicked');
      console.log('[TOTPer] Current entries:', entries);
      if (!entries.length) {
        alert('Add at least one entry first.');
        return;
      }
      alert('Configuration would be saved to watch!\\n\\n' + JSON.stringify(entries, null, 2));
    });

    document.getElementById('reset').addEventListener('click', function() {
      if (confirm('Delete all entries? This cannot be undone.')) {
        entries = [];
        renderEntries();
      }
    });

    document.getElementById('test-log').addEventListener('click', function() {
      console.log('[TOTPer] Test log button clicked');
      console.log('[TOTPer] Current entries:', entries);
      console.log('[TOTPer] Entries count:', entries.length);
    });

    renderEntries();
    console.log('[TOTPer] Test interface ready');
  })();
