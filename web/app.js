/* ==========================================================================
   Rivo — comportamiento del sitio estático
   Vanilla JS, sin dependencias ni build. Tema, menú, copiado y
   resaltado de sintaxis básico para archivos .ro.
   ========================================================================== */

(function () {
  "use strict";

  var THEME_KEY = "rivo-theme";

  /* ------------------------------------------------------------------------
     Tema (oscuro por defecto)
     ------------------------------------------------------------------------ */

  function initTheme() {
    var toggle = document.querySelector("[data-theme-toggle]");
    var stored = null;

    try {
      stored = localStorage.getItem(THEME_KEY);
    } catch (e) {
      stored = null;
    }

    if (stored === "light" || stored === "dark") {
      document.documentElement.setAttribute("data-theme", stored);
    } else {
      document.documentElement.setAttribute("data-theme", "dark");
    }

    if (!toggle) return;

    function refreshLabel() {
      var current = document.documentElement.getAttribute("data-theme") || "dark";
      toggle.setAttribute(
        "aria-label",
        current === "dark" ? "Cambiar al tema claro" : "Cambiar al tema oscuro"
      );
      toggle.textContent = current === "dark" ? "☀" : "☾";
    }

    refreshLabel();

    toggle.addEventListener("click", function () {
      var current = document.documentElement.getAttribute("data-theme") || "dark";
      var next = current === "dark" ? "light" : "dark";
      document.documentElement.setAttribute("data-theme", next);
      try {
        localStorage.setItem(THEME_KEY, next);
      } catch (e) {
        /* almacenamiento no disponible: el tema solo dura esta sesión */
      }
      refreshLabel();
    });
  }

  /* ------------------------------------------------------------------------
     Menú móvil
     ------------------------------------------------------------------------ */

  function initNav() {
    var button = document.querySelector("[data-nav-toggle]");
    var nav = document.querySelector("[data-site-nav]");
    if (!button || !nav) return;

    button.addEventListener("click", function () {
      var open = nav.classList.toggle("open");
      button.setAttribute("aria-expanded", open ? "true" : "false");
    });

    nav.addEventListener("click", function (event) {
      if (event.target.tagName === "A") {
        nav.classList.remove("open");
        button.setAttribute("aria-expanded", "false");
      }
    });
  }

  /* ------------------------------------------------------------------------
     Botones "copiar" en los bloques de código
     ------------------------------------------------------------------------ */

  function initCopyButtons() {
    var blocks = document.querySelectorAll("pre > code");
    Array.prototype.forEach.call(blocks, function (code) {
      var pre = code.parentNode;
      if (!pre || pre.querySelector(".copy-btn")) return;

      var btn = document.createElement("button");
      btn.type = "button";
      btn.className = "copy-btn";
      btn.textContent = "copiar";
      btn.setAttribute("aria-label", "Copiar el código al portapapeles");

      btn.addEventListener("click", function () {
        var text = code.textContent;

        function done(ok) {
          btn.textContent = ok ? "copiado" : "error";
          btn.classList.toggle("copied", ok);
          window.setTimeout(function () {
            btn.textContent = "copiar";
            btn.classList.remove("copied");
          }, 1600);
        }

        if (navigator.clipboard && navigator.clipboard.writeText) {
          navigator.clipboard.writeText(text).then(
            function () {
              done(true);
            },
            function () {
              done(fallbackCopy(text));
            }
          );
        } else {
          done(fallbackCopy(text));
        }
      });

      pre.appendChild(btn);
    });
  }

  function fallbackCopy(text) {
    var area = document.createElement("textarea");
    area.value = text;
    area.setAttribute("readonly", "");
    area.style.position = "fixed";
    area.style.opacity = "0";
    document.body.appendChild(area);
    area.select();
    var ok = false;
    try {
      ok = document.execCommand("copy");
    } catch (e) {
      ok = false;
    }
    document.body.removeChild(area);
    return ok;
  }

  /* ------------------------------------------------------------------------
     Resaltado de sintaxis de Rivo (.ro)
     ------------------------------------------------------------------------ */

  var KEYWORDS = [
    "fn", "return", "var", "const", "if", "else", "while", "for", "in",
    "break", "continue", "pass", "import", "from", "as", "and", "or", "not",
    "true", "false", "null", "try", "catch", "external"
  ];

  var TYPES = ["int", "float", "string", "bool", "char", "list", "map"];

  var TOKEN_RE = new RegExp(
    [
      "#\\[\\s\\S]*?!#",                       // comentario multilínea
      "#[^\\n]*",                              // comentario de una línea
      'f?"(?:\\\\.|[^"\\\\])*"',               // f-string o string
      "'(?:\\\\.|[^'\\\\])*'",                 // carácter
      "\\b\\d+\\.\\d+\\b",                     // float
      "\\b\\d+\\b",                            // entero
      "\\b[A-Za-z_][A-Za-z0-9_]*\\b"           // identificador / palabra clave
    ].join("|"),
    "g"
  );

  function escapeHtml(text) {
    return text
      .replace(/&/g, "&amp;")
      .replace(/</g, "&lt;")
      .replace(/>/g, "&gt;");
  }

  function classify(word) {
    if (KEYWORDS.indexOf(word) !== -1) return "tok-kw";
    if (TYPES.indexOf(word) !== -1) return "tok-type";
    return null;
  }

  function highlightRivo(source) {
    var out = "";
    var last = 0;
    var match;

    TOKEN_RE.lastIndex = 0;

    while ((match = TOKEN_RE.exec(source)) !== null) {
      var raw = match[0];
      var index = match.index;

      if (index > last) {
        out += escapeHtml(source.slice(last, index));
      }

      var cls;
      if (raw.charAt(0) === "#") {
        cls = "tok-comment";
      } else if (raw.charAt(0) === '"' || (raw.charAt(0) === "f" && raw.charAt(1) === '"')) {
        cls = "tok-str";
      } else if (raw.charAt(0) === "'") {
        cls = "tok-str";
      } else if (/^\d/.test(raw)) {
        cls = "tok-num";
      } else {
        var kind = classify(raw);
        if (kind) {
          cls = kind;
        } else {
          var rest = source.slice(index + raw.length);
          cls = /^\s*\(/.test(rest) ? "tok-fn" : null;
        }
      }

      out += cls
        ? '<span class="' + cls + '">' + escapeHtml(raw) + "</span>"
        : escapeHtml(raw);

      last = index + raw.length;
    }

    out += escapeHtml(source.slice(last));
    return out;
  }

  function initHighlighting() {
    var blocks = document.querySelectorAll("pre code[data-lang='rivo']");
    Array.prototype.forEach.call(blocks, function (code) {
      var source = code.textContent;
      code.innerHTML = highlightRivo(source);
    });
  }

  /* ------------------------------------------------------------------------
     Barra lateral de la documentación (scrollspy)
     ------------------------------------------------------------------------ */

  function initDocsSpy() {
    var sidebar = document.querySelector("[data-docs-sidebar]");
    if (!sidebar) return;

    var links = sidebar.querySelectorAll("a[href^='#']");
    if (!links.length || !("IntersectionObserver" in window)) return;

    var map = {};
    var targets = [];

    Array.prototype.forEach.call(links, function (link) {
      var id = link.getAttribute("href").slice(1);
      var section = document.getElementById(id);
      if (!section) return;
      map[id] = link;
      targets.push(section);
    });

    var visible = {};

    var observer = new IntersectionObserver(
      function (entries) {
        entries.forEach(function (entry) {
          visible[entry.target.id] = entry.isIntersecting;
        });

        var currentId = null;
        for (var i = 0; i < targets.length; i += 1) {
          if (visible[targets[i].id]) {
            currentId = targets[i].id;
            break;
          }
        }

        Array.prototype.forEach.call(links, function (link) {
          link.classList.remove("active");
          link.removeAttribute("aria-current");
        });

        if (currentId && map[currentId]) {
          map[currentId].classList.add("active");
          map[currentId].setAttribute("aria-current", "true");
        }
      },
      {
        rootMargin: "-20% 0px -65% 0px",
        threshold: 0
      }
    );

    targets.forEach(function (section) {
      observer.observe(section);
    });
  }

  /* ------------------------------------------------------------------------
     Botón "índice" de la docs en móvil
     ------------------------------------------------------------------------ */

  function initDocsToggle() {
    var button = document.querySelector("[data-docs-toggle]");
    var sidebar = document.querySelector("[data-docs-sidebar]");
    if (!button || !sidebar) return;

    var backdrop = document.createElement("div");
    backdrop.className = "docs-backdrop";
    backdrop.setAttribute("aria-hidden", "true");
    document.body.appendChild(backdrop);

    function setOpen(open) {
      sidebar.classList.toggle("open", open);
      backdrop.classList.toggle("open", open);
      button.setAttribute("aria-expanded", open ? "true" : "false");
      button.textContent = open ? "Ocultar índice" : "Índice de la documentación";
      document.body.style.overflow = open ? "hidden" : "";
    }

    button.addEventListener("click", function () {
      setOpen(!sidebar.classList.contains("open"));
    });

    backdrop.addEventListener("click", function () {
      setOpen(false);
    });

    document.addEventListener("keydown", function (event) {
      if (event.key === "Escape" && sidebar.classList.contains("open")) {
        setOpen(false);
        button.focus();
      }
    });

    sidebar.addEventListener("click", function (event) {
      if (event.target.closest && event.target.closest("a")) {
        setOpen(false);
      }
    });
  }

  /* ------------------------------------------------------------------------
     Cabecera con sombra al desplazar
     ------------------------------------------------------------------------ */

  function initHeaderScroll() {
    var header = document.querySelector(".site-header");
    if (!header) return;

    var ticking = false;

    function update() {
      header.classList.toggle("scrolled", window.scrollY > 8);
      ticking = false;
    }

    window.addEventListener(
      "scroll",
      function () {
        if (!ticking) {
          ticking = true;
          window.requestAnimationFrame(update);
        }
      },
      { passive: true }
    );

    update();
  }

  /* ------------------------------------------------------------------------
     Aparición de elementos al hacer scroll (revelado)
     ------------------------------------------------------------------------ */

  var REVEAL_SELECTOR = [
    ".section-head",
    ".card",
    ".steps li",
    ".callout",
    ".table-wrap",
    ".docs-content section",
    "ul.checklist"
  ].join(", ");

  function initReveal() {
    if (!("IntersectionObserver" in window)) return;

    var elements = document.querySelectorAll(REVEAL_SELECTOR);
    if (!elements.length) return;

    Array.prototype.forEach.call(elements, function (el) {
      el.classList.add("reveal");

      /* Escalona los hermanos que comparten contenedor. */
      var index = 0;
      var prev = el.previousElementSibling;
      while (prev) {
        if (prev.matches && prev.matches(REVEAL_SELECTOR)) index += 1;
        prev = prev.previousElementSibling;
      }
      el.style.setProperty("--reveal-delay", Math.min(index, 5) * 0.07 + "s");
    });

    var observer = new IntersectionObserver(
      function (entries) {
        entries.forEach(function (entry) {
          if (entry.isIntersecting) {
            entry.target.classList.add("is-visible");
            observer.unobserve(entry.target);
          }
        });
      },
      { rootMargin: "0px 0px -6% 0px", threshold: 0.04 }
    );

    Array.prototype.forEach.call(elements, function (el) {
      observer.observe(el);
    });
  }

  /* ------------------------------------------------------------------------
     Arranque
     ------------------------------------------------------------------------ */

  function start() {
    initTheme();
    initNav();
    initCopyButtons();
    initHighlighting();
    initDocsSpy();
    initDocsToggle();
    initHeaderScroll();
    initReveal();
  }

  if (document.readyState === "loading") {
    document.addEventListener("DOMContentLoaded", start);
  } else {
    start();
  }
})();
