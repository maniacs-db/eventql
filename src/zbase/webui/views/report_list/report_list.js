ZBase.registerView((function() {

  var load = function(url) {
    var qparams = {
      with_categories: true,
      type: "report",
      author: "all"
    };

    var owner_param = UrlUtil.getParamValue(url, "owner");
    if (owner_param) {
      qparams.owner = owner_param;
    }

    var author_param = UrlUtil.getParamValue(url, "author");
    if (author_param) {
      qparams.author = author_param;
    }

    var category_param = UrlUtil.getParamValue(url, "category");
    if (category_param) {
      qparams.category_prefix = category_param;
    }

    var pstatus_param = UrlUtil.getParamValue(url, "publishing_status");
    if (pstatus_param) {
      qparams.publishing_status = pstatus_param;
    }


    $.showLoader();
    $.httpGet("/api/v1/documents?" + $.buildQueryString(qparams), function(r) {
      if (r.status == 200) {
        render(JSON.parse(r.response), qparams, url);
      } else {
        $.fatalError();
      }
    });
  }

  var render = function(data, qparams, path) {
    var reports = data.documents;
    var categories = data.categories;

    var page = $.getTemplate(
        "views/report_list",
        "zbase_report_list_main_tpl");

    var main_menu = ZBaseMainMenu();
    main_menu.render($(".zbase_main_menu", page), path);

    renderTable(
        page.querySelector(".zbase_report_list tbody"),
        reports);

    $.onClick($("button.new_report", page), function(e) {
      $.createNewDocument("report");
    });

    $.handleLinks(page);
    $.replaceViewport(page)
    $.hideLoader();
  };

  var renderTable = function(tbody_elem, reports) {
    reports.forEach(function(doc) {
      var url = "/a/reports/" + doc.uuid;

      var tr = document.createElement("tr");
      tr.innerHTML = 
          "<td><a href='" + url + "'>" + $.escapeHTML(doc.name) + "</a></td>" +
          "<td><a href='" + url + "'>" + $.escapeHTML(doc.type) + "</a></td>" +
          "<td><a href='" + url + "'>" + DateUtil.printTimeAgo(doc.mtime) + "</a></td>";

      tbody_elem.appendChild(tr);
    });
  };

  return {
    name: "report_list",
    loadView: function(params) { load(params.path); },
    unloadView: function() {},
    handleNavigationChange: load
  };

})());