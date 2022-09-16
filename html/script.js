var wsOutputQueue = [];
var wsBusy = false;
var wsPaused = false;
var wsOutputQueueTimer = null;
var StatusRequestTimer = null;
var FseqFileListRequestTimer = null;
var ws = null; // Web Socket

// global data
var AdminInfo = null;
var Output_Config = null; // Output Manager configuration record
var Input_Config = null; // Input Manager configuration record
var System_Config = null;
var Fseq_File_List = null;
var selector = [];
var target = null;
var SdCardIsInstalled = false;
var FseqFileTransferStartTime = new Date();
var pingTimer;
var pongTimer;
var IsDocumentHidden = false;

// Drawing canvas - move to diagnostics
var canvas = document.getElementById("canvas");
var ctx = canvas.getContext("2d");
ctx.font = "20px Arial";
ctx.textAlign = "center";

// Default modal properties
$.fn.modal.Constructor.DEFAULTS.backdrop = 'static';
$.fn.modal.Constructor.DEFAULTS.keyboard = false;

// lets get started
wsConnect();

// console.log ('************before enqueue');
// wsEnqueue(JSON.stringify({ 'cmd': { 'get': 'system' } })); // Get general config
// console.log ('************after enqueue');

// jQuery doc ready
$(function () {
    // Menu navigation for single page layout
    $('ul.navbar-nav li a').click(function () {
        // Highlight proper navbar item
        $('.nav li').removeClass('active');
        $(this).parent().addClass('active');

        // Show the proper menu div
        $('.mdiv').addClass('hidden');
        $($(this).attr('href')).removeClass('hidden');

        ProcessWindowChange($($(this))[0].hash);

        // Collapse the menu on smaller screens
        $('#navbar').removeClass('in').attr('aria-expanded', 'false');
        $('.navbar-toggle').attr('aria-expanded', 'false');

        // Firmware selection and upload
        $('#efu').change(function () {
            let file = _('efu').files[0];
            let formdata = new FormData();
            formdata.append("file", file);
            let FileXfer = new XMLHttpRequest();

            FileXfer.upload.addEventListener("progress", progressHandler, false);
            FileXfer.addEventListener("load", completeHandler, false);
            FileXfer.addEventListener("error", errorHandler, false);
            FileXfer.addEventListener("abort", abortHandler, false);
            FileXfer.open("POST", "http://" + target + "/updatefw");
            FileXfer.send(formdata);
            $("#EfuProgressBar").removeClass("hidden");

            function _(el) {
                return document.getElementById(el);
            }
            function progressHandler(event) {
                let percent = (event.loaded / event.total) * 100;
                _("EfuProgressBar").value = Math.round(percent);
            }

            function completeHandler(event) {
                // _("status").innerHTML = event.target.responseText;
                _("EfuProgressBar").value = 0; //will clear progress bar after successful upload
                showReboot();
            }

            function errorHandler(event) {
                console.error("Transfer Error");
                // _("status").innerHTML = "Upload Failed";
            }

            function abortHandler(event) {
                console.error("Transfer Abort");
                // _("status").innerHTML = "Upload Aborted";
            }
        });
    });

    // DHCP field toggles
    $('#wifi #dhcp').change(function () {
        if ($(this).is(':checked')) {
            $('.dhcp').removeClass('hidden');
            $('.dhcp').addClass('hidden');
        }
        else {
            $('.dhcp').removeClass('hidden');
        }
        $('#btn_network').prop("disabled", ValidateConfigFields($("#network #wifi input")));
    });

    $('#eth #dhcp').change(function () {
        if ($(this).is(':checked')) {
            $('.ethdhcp').addClass('hidden');
        }
        else {
            $('.ethdhcp').removeClass('hidden');
        }
        $('#btn_network').prop("disabled", ValidateConfigFields($("#network #wifi input")));
    });

    $('#network').on("input", (function () {
        $('#btn_network').prop("disabled", ValidateConfigFields($("#network #wifi input")));
    }));

    $('#config').on("input", (function () {
        $('#DeviceConfigSave').prop("disabled", ValidateConfigFields($('#config input')));
    }));

    $('#DeviceConfigSave').click(function () {
        submitDeviceConfig();
    });

    $('#btn_network').click(function () {
        submitNetworkConfig();
    });

    $('#viewStyle').change(function () {
        clearStream();
    });

    $('#v_columns').on('input', function () {
        clearStream();
    });

    //TODO: This should pull a configuration from the stick and not the web interface as web data could be invalid
    $('#backupconfig').click(function () {
        ExtractNetworkConfigFromHtmlPage();
        ExtractChannelConfigFromHtmlPage(Input_Config.channels, "input");
        ExtractChannelConfigFromHtmlPage(Output_Config.channels, "output");
        System_Config.device.id = $('#config #device #id').val();
        System_Config.device.blanktime = $('#config #device #blanktime').val();

        let TotalConfig = JSON.stringify({ 'system': System_Config, 'input': Input_Config, 'output': Output_Config });

        let blob = new Blob([TotalConfig], { type: "text/json;charset=utf-8" });
        let FileName = System_Config.device.id.replace(".", "-").replace(" ", "-").replace(",", "-") + "-" + AdminInfo.flashchipid;
        saveAs(blob, FileName + ".json"); // Filesaver.js
    });

    $('#restoreconfig').change(function () {
        if (this.files.length !== 0) {
            const reader = new FileReader();
            reader.onload = function fileReadCompleted() {
                // when the reader is done, the content is in reader.result.
                ProcessLocalConfig(reader.result);
            };
            reader.readAsText(this.files[0]);
        }
    });

    $('#adminReboot').click(function () {
        reboot();
    });

    $('#AdvancedOptions').change(function () {
        UpdateAdvancedOptionsMode();
        UpdateChannelCounts();
    });

    let finalUrl = "http://" + target + "/upload";
    // console.log(finalUrl);
    const uploader = new Dropzone('#filemanagementupload',
        {
            url: finalUrl,
            paramName: 'file',
            maxFilesize: 1000, // MB
            maxFiles: 1,
            parallelUploads: 1,
            clickable: true,
            uploadMultiple: false,
            createImageThumbnails: false,
            dictDefaultMessage: 'Drag an image here to upload, or click to select one',
            acceptedFiles: '.fseq,.pl',
            timeout: 99999999, /*milliseconds*/
            init: function () {
                this.on('success', function (file, resp) {
                    // console.log("Success");
                    // console.log(file);
                    // console.log(resp);
                    Dropzone.forElement('#filemanagementupload').removeAllFiles(true)
                    RequestListOfFiles();
                });

                this.on('complete', function (file, resp) {
                    // console.log("complete");
                    // console.log(file);
                    // console.log(resp);
                    $('#fseqprogress_fg').addClass("hidden");

                    let DeltaTime = (new Date().getTime() - FseqFileTransferStartTime.getTime()) / 1000;
                    let rate = Math.floor((file.size / DeltaTime) / 1000);
                    console.debug("Final Transfer Rate: " + rate + "KBps");
                });

                this.on('addedfile', function (file, resp) {
                    // console.log("addedfile");
                    // console.log(file);
                    // console.log(resp);
                    FseqFileTransferStartTime = new Date();
                });

                this.on('uploadprogress', function (file, percentProgress, bytesSent) {
                    // console.log("percentProgress: " + percentProgress);
                    // console.log("bytesSent: " + bytesSent);
                    $('#fseqprogress_fg').removeClass("hidden");
                    $('#fseqprogressbytes').html(bytesSent);

                    let now = new Date().getTime();
                    let DeltaTime = (now - FseqFileTransferStartTime.getTime()) / 1000;
                    let rate = Math.floor((bytesSent / DeltaTime) / 1000);
                    $('#fseqprogressrate').html(rate + "KBps");
                });
            },

            accept: function (file, done) {
                // console.log("accept");
                // console.log(file);
                return done(); // triggers a send
            }
        });

    $("#filemanagementupload").addClass("dropzone");

    $('#FileDeleteButton').click(function () {
        RequestFileDeletion();
    });
    /*
        $('#FileUploadButton').click(function () {
            RequestFileUpload();
        });
    */
    // Autoload tab based on URL hash
    let hash = window.location.hash;
    hash && $('ul.navbar-nav li a[href="' + hash + '"]').click();

    // Halt pingpong if document is not visible
    document.addEventListener("visibilitychange", function () {
        if (document.hidden) {
            clearTimeout(pingTimer);
            clearTimeout(pongTimer);
            IsDocumentHidden = true;
        } else {
            IsDocumentHidden = false;
            wsReadyToSend();
            wsPingPong();
        }
    });
});

function ProcessLocalConfig(data) {
    // console.info(data);
    let ParsedLocalConfig = JSON.parse(data);

    wsEnqueue(JSON.stringify({ 'cmd': { 'set': { 'system': ParsedLocalConfig } } }));
    wsEnqueue(JSON.stringify({ 'cmd': { 'set': { 'input': { 'input_config': ParsedLocalConfig.input } } } }));
    wsEnqueue(JSON.stringify({ 'cmd': { 'set': { 'output': { 'output_config': ParsedLocalConfig.output } } } }));

} // ProcessLocalConfig

function UpdateAdvancedOptionsMode() {
    // console.info("UpdateAdvancedOptionsMode");

    let am = $('#AdvancedOptions');
    let AdvancedModeState = am.prop("checked");

    $(".AdvancedMode").each(function () {
        if (true === AdvancedModeState) {
            $(this).removeClass("hidden");
        }
        else {
            $(this).addClass("hidden");
        }
    });
} // UpdateAdvancedOptionsMode

function UpdateChannelCounts() {
    // console.info("UpdateChannelCounts");
    if (null !== Output_Config) {
        $(".SerialCount").each(function () {
            $(this).attr('max', Output_Config.TotalChannels);
        });

        $(".PixelCount").each(function () {
            $(this).attr('max', Output_Config.TotalChannels / 3);
        });
    }
} // UpdateChannelCounts

function ProcessWindowChange(NextWindow) {

    if (NextWindow === "#diag") {
        wsEnqueue('V1');
    }

    else if (NextWindow === "#admin") {
        wsEnqueue('XA');
        wsEnqueue(JSON.stringify({ 'cmd': { 'get': 'system' } })); // Get general config
        wsEnqueue(JSON.stringify({ 'cmd': { 'get': 'output' } })); // Get output config
        wsEnqueue(JSON.stringify({ 'cmd': { 'get': 'input' } }));  // Get input config
    }

    else if ((NextWindow === "#pg_network") || (NextWindow === "#home")) {
        wsEnqueue(JSON.stringify({ 'cmd': { 'get': 'system' } })); // Get general config
    }

    else if (NextWindow === "#config") {
        RequestListOfFiles();
        wsEnqueue(JSON.stringify({ 'cmd': { 'get': 'system' } })); // Get general config
        wsEnqueue(JSON.stringify({ 'cmd': { 'get': 'output' } })); // Get output config
        wsEnqueue(JSON.stringify({ 'cmd': { 'get': 'input' } }));  // Get input config
    }

    else if (NextWindow === "#filemanagement") {
        RequestListOfFiles();
    }

    UpdateAdvancedOptionsMode();
    UpdateChannelCounts();

} // ProcessWindowChange

function RequestStatusUpdate() {
    // is the timer running?
    if (null === StatusRequestTimer) {
        // timer runs forever
        StatusRequestTimer = setTimeout(function () {
            clearTimeout(StatusRequestTimer);
            StatusRequestTimer = null;

            RequestStatusUpdate();

        }, 1000);
    } // end timer was not running

    if ($('#home').is(':visible')) {
        // ask for a status update from the server
        wsEnqueue('XJ');
    } // end home (aka status) is visible

} // RequestStatusUpdate

function RequestListOfFiles() {
    // is the timer running?
    if (null === FseqFileListRequestTimer) {
        // timer runs until we get a response
        FseqFileListRequestTimer = setTimeout(function () {
            clearTimeout(FseqFileListRequestTimer);
            FseqFileListRequestTimer = null;

            RequestListOfFiles();

        }, 1000);
    } // end timer was not running

    // ask for a file list from the server
    wsEnqueue(JSON.stringify({ 'cmd': { 'get': 'files' } })); // Get File List

} // RequestListOfFiles

function BytesToMB(Value) {
    return (Value / (1024 * 1024)).toFixed();

} // BytesToMB

function ProcessGetFileListResponse(JsonConfigData) {
    // console.info("ProcessGetFileListResponse");

    SdCardIsInstalled = JsonConfigData.SdCardPresent;

    $("#li-filemanagement").removeClass("hidden");
    if (false === SdCardIsInstalled) {
        $("#li-filemanagement").addClass("hidden");
    }

    $("#totalBytes").val(BytesToMB(JsonConfigData.totalBytes));
    $("#usedBytes").val(BytesToMB(JsonConfigData.usedBytes));
    $("#remainingBytes").val(BytesToMB(JsonConfigData.totalBytes - JsonConfigData.usedBytes));

    Fseq_File_List = JsonConfigData;

    clearTimeout(FseqFileListRequestTimer);
    FseqFileListRequestTimer = null;

    // console.info("$('#FileManagementTable > tr').length " + $('#FileManagementTable > tr').length);

    while (1 < $('#FileManagementTable > tr').length) {
        // console.info("Deleting $('#FileManagementTable tr').length " + $('#FileManagementTable tr').length);
        $('#FileManagementTable tr').last().remove();
        // console.log("After Delete: $('#FileManagementTable tr').length " + $('#FileManagementTable tr').length);
    }

    let CurrentRowId = 0;
    JsonConfigData.files.forEach(function (file) {
        let SelectedPattern = '<td><input  type="checkbox" id="FileSelected_' + (CurrentRowId) + '"></td>';
        let NamePattern = '<td><output type="text"     id="FileName_' + (CurrentRowId) + '"></td>';
        let DatePattern = '<td><output type="text"     id="FileDate_' + (CurrentRowId) + '"></td>';
        let SizePattern = '<td><output type="text"     id="FileSize_' + (CurrentRowId) + '"></td>';

        let rowPattern = '<tr>' + SelectedPattern + NamePattern + DatePattern + SizePattern + '</tr>';
        $('#FileManagementTable tr:last').after(rowPattern);

        try {
            $('#FileName_' + (CurrentRowId)).val(file.name);
            $('#FileDate_' + (CurrentRowId)).val(new Date(file.date * 1000).toLocaleString());
            $('#FileSize_' + (CurrentRowId)).val(file.length);
        }
        catch
        {
            $('#FileName_' + (CurrentRowId)).val("InvalidFile");
            $('#FileDate_' + (CurrentRowId)).val(new Date(0).toISOString());
            $('#FileSize_' + (CurrentRowId)).val(0);
        }

        CurrentRowId++;
    });
} // ProcessGetFileListResponse

function RequestFileDeletion() {
    let files = [];

    $('#FileManagementTable > tr').each(function (CurRowId) {
        if (true === $('#FileSelected_' + CurRowId).prop("checked")) {
            let FileEntry = {};
            FileEntry["name"] = $('#FileName_' + CurRowId).val().toString();
            files.push(FileEntry);
        }
    });

    wsEnqueue(JSON.stringify({ 'cmd': { 'delete': { 'files': files } } }));
    RequestListOfFiles();

} // RequestFileDeletion

/*
function RequestFileUpload()
{
    $('#FileManagementTable > tr').each(function (CurRowId)
    {
        if (true === $('#FileSelected_' + CurRowId).prop("checked"))
        {
            let FileName   = $('#FileName_' + CurRowId).val().toString().replace(" - ", "/");
            let FileLength = parseInt($('#Length_'   + CurRowId).val());
            let uri = "data:application/octet-stream";
            console.info("       uri: " + uri);
            console.info("  FileName: " + FileName);
            console.info("FileLength: " + FileName);
            downloadURI(uri, FileName, FileLength);
            $('#FileSelected_' + CurRowId).prop("checked", false);
        }
    });

} // RequestFileUpload
*/
/*
async function downloadURI(uri, name, totalLength)
{

    const response = await fetch('http://' + target + '/download/' + name);
            .then(resp => resp.blob())

    let length = response.headers.get('Content-Length');
    console.info("length: " + length);

    if (!length)
    {
        length = totalLength; // handle the error
        console.info("Adjusted length: " + length);
    }

    console.info("response.status: " + response.status);
    if (response.status >= 200 && response.status < 300)
    {
        let results = await response.json();
    }
    else
    {
        alert("Download '" + name + "' request was rejected by server");
    }

/*
    window.status = "Download '" + name + "' Started";
    fetch('http://' + target + '/download/' + name)
        .then(resp => resp.blob())
        .then(blob => {
            const url = window.URL.createObjectURL(blob);
            const a = document.createElement('a');
            a.style.display = 'none';
            a.href = url;
            // the filename you want
            a.download = name;
            document.body.appendChild(a);
            a.click();
            window.URL.revokeObjectURL(url);
            window.status = "Download '" + name + "' Complete";
        })
        .catch(() => alert("Download '" + name + "' Failed"));
* /
} // downloadURI
*/

function ParseParameter(name) {
    return (location.search.split(name + '=')[1] || '').split('&')[0];
}

function ProcessModeConfigurationDatafppremote(channelConfig) {
    let jqSelector = "#fseqfilename";

    // remove the existing options
    $(jqSelector).empty();

    $(jqSelector).append('<option value="...">Play Remote Sequence</option>');

    // for each file in the list
    Fseq_File_List.files.forEach(function (listEntry) {
        // add in a new entry
        $(jqSelector).append('<option value="' + listEntry.name + '">' + listEntry.name + '</option>');
    });

    // set the current selector value
    $(jqSelector).val(channelConfig.fseqfilename);


} // ProcessModeConfigurationDatafppremote

function ProcessModeConfigurationDataEffects(channelConfig) {
    let jqSelector = "#currenteffect";

    // remove the existing options
    $(jqSelector).empty();

    // for each option in the list
    channelConfig.effects.forEach(function (listEntry) {
        // add in a new entry
        $(jqSelector).append('<option value="' + listEntry.name + '">' + listEntry.name + '</option>');
    });

    // set the current selector value
    $(jqSelector).val(channelConfig.currenteffect);

} // ProcessModeConfigurationDataEffects

function ProcessModeConfigurationDataFishTank(channelConfig) {
    let jqSelector = "#ftm_mode";

    // remove the existing options
    $(jqSelector).empty();

    // for each option in the list
    channelConfig.modeTable.forEach(function (listEntry) {
        $(jqSelector).append('<option id="mode_' + listEntry.id + '" name="' + listEntry.id + '">' + listEntry.name + '</option>');
        if ("Cycle" !== listEntry.name) {
            // add in a new entry

            // console.log("CurrentRowId = " + CurrentRowId);
            let NamePattern = '<td id="FtMode_Name_' + (listEntry.id) + '" class="col-sm-2">' + listEntry.name + '</td>';
            let RedPattern = '<td><input type="number" id="FtMode_red_' + (listEntry.id) + '"step="1" min="0" max="255" value="' + listEntry.red + '" class="form-control is-valid col-sm-2"></td>';
            let GreenPattern = '<td><input type="number" id="FtMode_green_' + (listEntry.id) + '"step="1" min="0" max="255" value="' + listEntry.green + '" class="form-control is-valid col-sm-2"></td>';
            let BluePattern = '<td><input type="number" id="FtMode_blue_' + (listEntry.id) + '"step="1" min="0" max="255" value="' + listEntry.blue + '" class="form-control is-valid col-sm-2"></td>';

            let rowPattern = '<tr>' + NamePattern + RedPattern + GreenPattern + BluePattern + '</tr>';

            $('#FtModeConfigurationTable tr:last').after(rowPattern);

            $('#FtMode_Name_' + listEntry.id).attr('style', $('#FtMode_Name_hr').attr('style'));
            $('#FtMode_green_' + listEntry.id).attr('style', $('#FtMode_green_hr').attr('style'));
            $('#FtMode_blue_' + listEntry.id).attr('style', $('#FtMode_blue_hr').attr('style'));
        }
    });

    // set the current selector value
    $(jqSelector).val(channelConfig.modeTable[channelConfig.mode].name);

    channelConfig.timeToColor.forEach(function (listEntry) {

        let hourBaseId = "ftmHour_" + listEntry.id;
        let jqSelector = '#' + hourBaseId;

        channelConfig.modeTable.forEach(function (modeEntry) {
            if ("Cycle" !== modeEntry.name) {
                $(jqSelector).append('<option id="' + hourBaseId + '_' + modeEntry.id + '" name="' + modeEntry.id + '">' + modeEntry.name + '</option>');
            }
        });

        $(jqSelector).val(channelConfig.modeTable[listEntry.mode].name);
    });

} // ProcessModeConfigurationDataFishTank

function ProcessModeConfigurationDataRelay(RelayConfig) {
    // console.log("relaychannelconfigurationtable.rows.length = " + $('#relaychannelconfigurationtable tr').length);

    let ChannelConfigs = RelayConfig.channels;

    let HasPwmFrequency = false;
    if ({}.hasOwnProperty.call(ChannelConfigs[0], "Frequency")) {
        HasPwmFrequency = true;
        $("#Frequency_hr").removeClass("hidden");
    }
    else {
        $("#Frequency_hr").addClass("hidden");
    }

    // add as many rows as we need
    for (let CurrentRowId = 1; CurrentRowId <= ChannelConfigs.length; CurrentRowId++) {
        // console.log("CurrentRowId = " + CurrentRowId);

        let ChanIdPattern = '<td id="chanId_' + (CurrentRowId) + '">a</td>';
        let EnabledPattern = '<td><input type="checkbox" id="Enabled_' + (CurrentRowId) + '"></td>';
        let InvertedPattern = '<td><input type="checkbox" id="Inverted_' + (CurrentRowId) + '"></td>';
        let PwmPattern = '<td><input type="checkbox" id="Pwm_' + (CurrentRowId) + '"></td>';
        let gpioPattern = '<td><input type="number"   id="gpioId_' + (CurrentRowId) + '"step="1" min="0" max="34"  value="34"  class="form-control is-valid"></td>';
        let threshholdPattern = '<td><input type="number"   id="threshhold_' + (CurrentRowId) + '"step="1" min="0" max="255" value="300" class="form-control is-valid"></td>';
        let PwmFreqPattern = '';
        if (true === HasPwmFrequency) {
            PwmFreqPattern = '<td><input type="number"   id="Frequency_' + (CurrentRowId) + '"step="1" min="100" max="19000" value="19000" class="form-control is-valid"></td>';
        }

        let rowPattern = '<tr>' + ChanIdPattern + EnabledPattern + InvertedPattern + PwmPattern + gpioPattern + threshholdPattern + PwmFreqPattern + '</tr>';
        $('#relaychannelconfigurationtable tr:last').after(rowPattern);

        $('#chanId_' + CurrentRowId).attr('style', $('#chanId_hr').attr('style'));
        $('#Enabled_' + CurrentRowId).attr('style', $('#Enabled_hr').attr('style'));
        $('#Inverted_' + CurrentRowId).attr('style', $('#Inverted_hr').attr('style'));
        $('#Pwm_' + CurrentRowId).attr('style', $('#Pwm_hr').attr('style'));
        $('#gpioId_' + CurrentRowId).attr('style', $('#gpioId_hr').attr('style'));
        $('#threshhold_' + CurrentRowId).attr('style', $('#threshhold_hr').attr('style'));
        if (true === HasPwmFrequency) {
            $('#Frequency_' + CurrentRowId).attr('style', $('#Frequency_hr').attr('style'));
        }
    }

    // populate config
    $.each(ChannelConfigs, function (i, CurrentChannelConfig) {
        // console.log("Current Channel Id = " + CurrentChannelConfig.id);
        let currentChannelRowId = CurrentChannelConfig.id + 1;
        $('#chanId_' + (currentChannelRowId)).html(currentChannelRowId);
        $('#Enabled_' + (currentChannelRowId)).prop("checked", CurrentChannelConfig.en);
        $('#Inverted_' + (currentChannelRowId)).prop("checked", CurrentChannelConfig.inv);
        $('#Pwm_' + (currentChannelRowId)).prop("checked", CurrentChannelConfig.pwm);
        $('#gpioId_' + (currentChannelRowId)).val(CurrentChannelConfig.gid);
        $('#threshhold_' + (currentChannelRowId)).val(CurrentChannelConfig.trig);
        if (true === HasPwmFrequency) {
            $('#Frequency_' + (currentChannelRowId)).val(CurrentChannelConfig.Frequency);
        }
    });

} // ProcessModeConfigurationDataRelay

function ProcessModeConfigurationDataServoPCA9685(ServoConfig) {
    // console.log("Servochannelconfigurationtable.rows.length = " + $('#servo_pca9685channelconfigurationtable tr').length);

    let ChannelConfigs = ServoConfig.channels;

    // add as many rows as we need
    for (let CurrentRowId = 1; CurrentRowId <= ChannelConfigs.length; CurrentRowId++) {
        // console.log("CurrentRowId = " + CurrentRowId);
        let ChanIdPattern = '<td                        id="ServoChanId_' + (CurrentRowId) + '">a</td>';
        let EnabledPattern = '<td><input type="checkbox" id="ServoEnabled_' + (CurrentRowId) + '"></td>';
        let MinLevelPattern = '<td><input type="number"   id="ServoMinLevel_' + (CurrentRowId) + '"step="1" min="10" max="4095"  value="0"  class="form-control is-valid"></td>';
        let MaxLevelPattern = '<td><input type="number"   id="ServoMaxLevel_' + (CurrentRowId) + '"step="1" min="10" max="4095"  value="0"  class="form-control is-valid"></td>';
        let RestingPattern = '<td><input type="number"   id="ServoHomeValue_' + (CurrentRowId) + '"step="1" min="0"  max="255"   value="0"  class="form-control is-valid"></td>';
        let DataType = '<td><select class="form-control is-valid" id="ServoDataType_' + (CurrentRowId) + '" title="Effect to generate"></select></td>';

        let rowPattern = '<tr>' + ChanIdPattern + EnabledPattern + MinLevelPattern + MaxLevelPattern + DataType + RestingPattern + '</tr>';

        $('#servo_pca9685channelconfigurationtable tr:last').after(rowPattern);

        $('#ServoChanId_' + CurrentRowId).attr('style', $('#ServoChanId_hr').attr('style'));
        $('#ServoEnabled_' + CurrentRowId).attr('style', $('#ServoEnabled_hr').attr('style'));
        $('#ServoMinLevel_' + CurrentRowId).attr('style', $('#ServoMinLevel_hr').attr('style'));
        $('#ServoMaxLevel_' + CurrentRowId).attr('style', $('#ServoMaxLevel_hr').attr('style'));
        $('#ServoHomeValue_' + CurrentRowId).attr('style', $('#ServoHomeValue_hr').attr('style'));
        $('#ServoDataType_' + CurrentRowId).attr('style', $('#ServoDataType_hr').attr('style'));
    }

    $.each(ChannelConfigs, function (i, CurrentChannelConfig) {
        // console.log("Current Channel Id = " + CurrentChannelConfig.id);
        let currentChannelRowId = CurrentChannelConfig.id + 1;
        $('#ServoChanId_' + (currentChannelRowId)).html(currentChannelRowId);
        $('#ServoEnabled_' + (currentChannelRowId)).prop("checked", CurrentChannelConfig.en);
        $('#ServoMinLevel_' + (currentChannelRowId)).val(CurrentChannelConfig.Min);
        $('#ServoMaxLevel_' + (currentChannelRowId)).val(CurrentChannelConfig.Max);
        $('#ServoHomeValue_' + (currentChannelRowId)).val(CurrentChannelConfig.hv);

        let jqSelector = "#ServoDataType_" + (currentChannelRowId);

        // remove the existing options
        $(jqSelector).empty();
        $(jqSelector).append('<option value=0> 8 Bit Absolute</option>');
        $(jqSelector).append('<option value=1> 8 Bit Absolute Reversed</option>');
        $(jqSelector).append('<option value=2> 8 Bit Scaled</option>');
        $(jqSelector).append('<option value=3> 8 Bit Scaled - Reversed</option>');
        $(jqSelector).append('<option value=4>16 Bit Absolute</option>');
        $(jqSelector).append('<option value=5>16 Bit Absolute - Reversed</option>');
        $(jqSelector).append('<option value=6>16 Bit Scaled</option>');
        $(jqSelector).append('<option value=7>16 Bit Scaled - Reversed</option>');

        // set the current selector value
        $(jqSelector).val((CurrentChannelConfig.rev << 0) +
            (CurrentChannelConfig.sca << 1) +
            (CurrentChannelConfig.b16 << 2));
    });

} // ProcessModeConfigurationDataServoPCA9685

function ProcessInputConfig() {
    $("#ecb_enable").prop("checked", Input_Config.ecb.enabled);
    $("#ecb_gpioid").val(Input_Config.ecb.id);
    $("#ecb_polarity").val(Input_Config.ecb.polarity);

} // ProcessInputConfig

function ProcessModeConfigurationData(channelId, ChannelType, JsonConfig) {
    // console.info("ProcessModeConfigurationData: Start");

    // determine the type of in/output that has been selected and populate the form
    let TypeOfChannelId = parseInt($('#' + ChannelType + channelId + " option:selected").val(), 10);
    let channelConfigSet = JsonConfig.channels[channelId];

    if (isNaN(TypeOfChannelId)) {
        // use the value we got from the controller
        TypeOfChannelId = channelConfigSet.type;
    }
    let channelConfig = channelConfigSet[TypeOfChannelId];
    let ChannelTypeName = channelConfig.type.toLowerCase();
    ChannelTypeName = ChannelTypeName.replace(".", "_");
    ChannelTypeName = ChannelTypeName.replace(" ", "_");
    // console.info("ChannelTypeName: " + ChannelTypeName);

    let elementids = [];
    let modeControlName = '#' + ChannelType + 'mode' + channelId;
    // console.info("modeControlName: " + modeControlName);

    // modify page title
    //TODO: Dirty hack to clean-up input names
    if (ChannelType !== 'input') {
        let ModeDisplayName = GenerateInputOutputControlLabel(ChannelType, channelId) + " - " + $(modeControlName + ' #Title')[0].innerHTML;
        // console.info("ModeDisplayName: " + ModeDisplayName);
        $(modeControlName + ' #Title')[0].innerHTML = ModeDisplayName;
    }

    elementids = $(modeControlName + ' *[id]').filter(":input").map(function () {
        return $(this).attr('id');
    }).get();

    elementids.forEach(function (elementid) {
        let SelectedElement = modeControlName + ' #' + elementid;
        if ($(SelectedElement).is(':checkbox')) {
            $(SelectedElement).prop('checked', channelConfig[elementid]);
        }
        else {
            $(SelectedElement).val(channelConfig[elementid]);
        }
    });

    if ("fpp_remote" === ChannelTypeName) {
        if (null !== Fseq_File_List) {
            ProcessModeConfigurationDatafppremote(channelConfig);
        }
    }

    else if ("effects" === ChannelTypeName) {
        ProcessModeConfigurationDataEffects(channelConfig);
    }

    else if ("relay" === ChannelTypeName) {
        // console.info("ProcessModeConfigurationData: relay");
        ProcessModeConfigurationDataRelay(channelConfig);
    }

    else if ("servo_pca9685" === ChannelTypeName) {
        // console.info("ProcessModeConfigurationData: servo");
        ProcessModeConfigurationDataServoPCA9685(channelConfig);
    }
    else if ("fish_tank" === ChannelTypeName) {
        ProcessModeConfigurationDataFishTank(channelConfig);
    }

    UpdateAdvancedOptionsMode();
    UpdateChannelCounts();

    // console.info("ProcessModeConfigurationData: End");

} // ProcessModeConfigurationData

function ProcessReceivedJsonConfigMessage(JsonConfigData) {
    // console.info("ProcessReceivedJsonConfigMessage: Start");

    // is this an output config?
    if ({}.hasOwnProperty.call(JsonConfigData, "output_config")) {
        // save the config for later use.
        Output_Config = JsonConfigData.output_config;
        CreateOptionsFromConfig("output", Output_Config);
    }

    // is this an input config?
    else if ({}.hasOwnProperty.call(JsonConfigData, "input_config")) {
        // save the config for later use.
        Input_Config = JsonConfigData.input_config;
        CreateOptionsFromConfig("input", Input_Config);
    }

    // is this a device config?
    else if ({}.hasOwnProperty.call(JsonConfigData, "system")) {
        System_Config = JsonConfigData.system;
        // console.info("Got System Config: " + System_Config);

        updateFromJSON(System_Config);

        if ({}.hasOwnProperty.call(System_Config.network, 'eth')) {
            $('#pg_network #network #eth').removeClass("hidden")
        }
        else {
            $('#pg_network #network #eth').addClass("hidden")
        }

    }

    // is this a file list?
    else if ({}.hasOwnProperty.call(JsonConfigData, "files")) {
        ProcessGetFileListResponse(JsonConfigData);
    }

    // is this an ACK response?
    else if ({}.hasOwnProperty.call(JsonConfigData, "OK")) {
        // console.info("Received Acknowledgement to config set command.")
    }

    else {
        console.error("unknown configuration record type has been ignored.")
    }

    // console.info("ProcessReceivedJsonConfigMessage: Done");

} // ProcessReceivedJsonConfigMessage

// Builds jQuery selectors from JSON data and updates the web interface
function updateFromJSON(obj) {
    for (let k in obj) {
        selector.push('#' + k);
        if (typeof obj[k] === 'object' && obj[k] !== null) {
            updateFromJSON(obj[k]);
        }
        else {
            let jqSelector = selector.join(' ');
            if (typeof obj[k] === 'boolean') {
                $(jqSelector).prop('checked', obj[k]);
            }
            else {
                $(jqSelector).val(obj[k]);
            }

            // Trigger keyup / change events
            $(jqSelector).trigger('keyup');
            $(jqSelector).trigger('change');
        }
        selector.pop();
    }

    // Update Device ID in footer
    $('#device-id').text($('#config #id').val());
}

function GenerateInputOutputControlLabel(OptionListName, DisplayedChannelId) {
    let Id = parseInt(DisplayedChannelId) + 1;
    let NewName = '';
    //TODO: Dirty Hack to clean-up Input lables
    if (OptionListName === `input`) {
        NewName = (Id === 1) ? 'Primary Input' : 'Secondary Input'
    } else {
        NewName = OptionListName.charAt(0).toUpperCase() + OptionListName.slice(1) + " " + Id;
    }
    // console.log(`IO Label: ${NewName}`)
    return NewName;

} // GenerateInputOutputControlLabel

function LoadDeviceSetupSelectedOption(OptionListName, DisplayedChannelId) {
    // console.info("OptionListName: " + OptionListName);
    // console.info("DisplayedChannelId: " + DisplayedChannelId);

    let HtmlLoadFileName = $('#' + OptionListName + DisplayedChannelId + ' option:selected').text().toLowerCase();
    // console.info("Base HtmlLoadFileName: " + HtmlLoadFileName);
    HtmlLoadFileName = HtmlLoadFileName.replace(".", "_");
    HtmlLoadFileName = HtmlLoadFileName.replace(" ", "_");
    HtmlLoadFileName = HtmlLoadFileName + ".html";
    // console.info("Adjusted HtmlLoadFileName: " + HtmlLoadFileName);

    //TODO: Detect modules that don't require configuration - DDP, Alexa, ?
    if ("disabled.html" === HtmlLoadFileName) {
        $('#' + OptionListName + 'mode' + DisplayedChannelId).empty();
        $('#refresh').html('-');
    }
    else {
        // try to load the field definition file for this channel type
        $('#' + OptionListName + 'mode' + DisplayedChannelId).load(HtmlLoadFileName, function () {
            if ("input" === OptionListName) {
                ProcessInputConfig();
                ProcessModeConfigurationData(DisplayedChannelId, OptionListName, Input_Config);
            }
            else if ("output" === OptionListName) {
                ProcessModeConfigurationData(DisplayedChannelId, OptionListName, Output_Config);

                // Trigger refresh update for outputs
                $('#fg_output_mode input').trigger('change');
            }
        });
    }

} // LoadDeviceSetupSelectedOption

function CreateOptionsFromConfig(OptionListName, Config) {
    // console.info("CreateOptionsFromConfig");

    // Set selection column width based on arch which equates to number of outputs for now
    let col = (AdminInfo.arch === 'ESP8266') ? '4' : '2';
    let Channels = Config.channels;

    if ("input" === OptionListName) {
        $('#ecpin').val(Config.ecpin);
    }

    // for each field we need to populate (input vs output)
    Object.keys(Channels).forEach(function (ChannelId) {
        // OptionListName is 'input' or 'output'
        // console.info("ChannelId: " + ChannelId);
        let CurrentChannel = Channels[ChannelId];

        // does the selection box we need already exist?
        if (!$('#' + OptionListName + 'mode' + ChannelId).length) {
            // console.log(`OptionListName: ${OptionListName}`)
            // create the selection box
            $(`#fg_${OptionListName}`).append(`<label class="control-label col-sm-2" for="${OptionListName}${ChannelId}">${GenerateInputOutputControlLabel(OptionListName, ChannelId)}</label>`);
            $(`#fg_${OptionListName}`).append(`<div class="col-sm-${col}"><select class="form-control wsopt" id="${OptionListName}${ChannelId}"></select></div>`);
            $(`#fg_${OptionListName}_mode`).append(`<fieldset id="${OptionListName}mode${ChannelId}"></fieldset>`);

        }

        let jqSelector = "#" + OptionListName + ChannelId;

        // remove the existing options
        $(jqSelector).empty();

        // for each Channel type in the list
        Object.keys(CurrentChannel).forEach(function (SelectionTypeId) {
            // console.info("SelectionId: " + SelectionTypeId);
            if ("type" === SelectionTypeId) {
                // console.info("Set the selector type to: " + CurrentChannel.type);
                $(jqSelector).val(CurrentChannel.type);
                LoadDeviceSetupSelectedOption(OptionListName, ChannelId);
                $(jqSelector).change(function () {
                    // console.info("Set the selector type to: " + CurrentChannel.type);
                    LoadDeviceSetupSelectedOption(OptionListName, ChannelId);
                });
            }
            else {
                let CurrentSection = CurrentChannel[SelectionTypeId];
                // console.info("Add '" + CurrentSection.type + "' to selector");
                $(jqSelector).append('<option value="' + SelectionTypeId + '">' + CurrentSection.type + '</option>');
            }
        }); // end for each selection type
    }); // end for each channel
} // CreateOptionsFromConfig

// Builds JSON config submission for "WiFi" tab
function ExtractNetworkWiFiConfigFromHtmlPage() {
    let wifi = System_Config.network.wifi;
    wifi.ssid = $('#network #wifi #ssid').val();
    wifi.passphrase = $('#network #wifi #passphrase').val();
    wifi.sta_timeout = $('#network #wifi #sta_timeout').val();
    wifi.ip = $('#network #wifi #ip').val();
    wifi.netmask = $('#network #wifi #netmask').val();
    wifi.gateway = $('#network #wifi #gateway').val();
    wifi.dhcp = $('#network #wifi #dhcp').prop('checked');
    wifi.ap_fallback = $('#network #wifi #ap_fallback').prop('checked');
    wifi.ap_reboot = $('#network #wifi #ap_reboot').prop('checked');
    wifi.ap_timeout = $('#network #wifi #ap_timeout').val();

} // ExtractNetworkWiFiConfigFromHtmlPage

function ExtractNetworkEthernetConfigFromHtmlPage() {
    if ({}.hasOwnProperty.call(System_Config.network, "eth")) {
        System_Config.network.weus = $('#network #eth #weus').prop('checked');

        System_Config.network.eth.ip = $('#network #eth #ip').val();
        System_Config.network.eth.netmask = $('#network #eth #netmask').val();
        System_Config.network.eth.gateway = $('#network #eth #gateway').val();
        System_Config.network.eth.dhcp = $('#network #eth #dhcp').prop('checked');
        System_Config.network.eth.type = parseInt($('#network #eth #type option:selected').val(), 10);
        System_Config.network.eth.addr = $('#network #eth #addr').val();
        System_Config.network.eth.power_pin = $('#network #eth #power_pin').val();
        System_Config.network.eth.mode = parseInt($('#network #eth #mode option:selected').val(), 10);
        System_Config.network.eth.mdc_pin = $('#network #eth #mdc_pin').val();
        System_Config.network.eth.mdio_pin = $('#network #eth #mdio_pin').val();
        System_Config.network.eth.activevalue = (parseInt($('#network #eth #activevalue option:selected').val(), 10) === 1);
        System_Config.network.eth.activedelay = $('#network #eth #activedelay').val();
    }

} // ExtractNetworkEthernetConfigFromHtmlPage

// Builds JSON config submission for "Network" tab
function ExtractNetworkConfigFromHtmlPage() {
    System_Config.network.hostname = $('#hostname').val();

    ExtractNetworkWiFiConfigFromHtmlPage();
    ExtractNetworkEthernetConfigFromHtmlPage();

} // ExtractNetworkConfigFromHtmlPage

// Builds JSON config submission for "WiFi" tab
function submitNetworkConfig() {
    System_Config.device.id = $('#config #device #id').val();
    System_Config.device.blanktime = $('#config #device #blanktime').val();
    System_Config.device.miso_pin = $('#config #device #miso_pin').val();
    System_Config.device.mosi_pin = $('#config #device #mosi_pin').val();
    System_Config.device.clock_pin = $('#config #device #clock_pin').val();
    System_Config.device.cs_pin = $('#config #device #cs_pin').val();

    ExtractNetworkConfigFromHtmlPage();

    // console.info("Send: " + JSON.stringify({ 'cmd': { 'set': { 'system': System_Config } } }));
    wsEnqueue(JSON.stringify({ 'cmd': { 'set': { 'system': System_Config } } }));

} // submitNetworkConfig

function ExtractRelayConfigFromHtmlPage(ChannelConfig, SectionName) {

    ChannelConfig.updateinterval = parseInt($('#updateinterval').val(), 10);

    $.each(ChannelConfig.channels, function (i, CurrentChannelConfig) {

        // console.info("Current Channel Id = " + CurrentChannelConfig.id);
        let currentChannelRowId = CurrentChannelConfig.id + 1;

        CurrentChannelConfig.en = $('#Enabled_' + (currentChannelRowId)).prop("checked");
        CurrentChannelConfig.inv = $('#Inverted_' + (currentChannelRowId)).prop("checked");
        CurrentChannelConfig.pwm = $('#Pwm_' + (currentChannelRowId)).prop("checked");
        CurrentChannelConfig.gid = parseInt($('#gpioId_' + (currentChannelRowId)).val(), 10);
        CurrentChannelConfig.trig = parseInt($('#threshhold_' + (currentChannelRowId)).val(), 10);

        if ({}.hasOwnProperty.call(ChannelConfig, "Frequency")) {
            CurrentChannelConfig.Frequency = parseInt($('#Frequency_' + (currentChannelRowId)).val(), 10);
        }
    });
} // ExtractRelayConfigFromHtmlPage

function ExtractPCA9685ConfigFromHtmlPage(ChannelConfig, SectionName) {

    ChannelConfig.updateinterval = parseInt($('#updateinterval').val(), 10);
    $.each(ChannelConfig.channels, function (i, CurrentChannelConfig) {
        // console.info("Current Channel Id = " + CurrentChannelConfig.id);
        let currentChannelRowId = CurrentChannelConfig.id + 1;
        CurrentChannelConfig.en = $('#ServoEnabled_' + (currentChannelRowId)).prop("checked");
        CurrentChannelConfig.Min = parseInt($('#ServoMinLevel_' + (currentChannelRowId)).val(), 10);
        CurrentChannelConfig.Max = parseInt($('#ServoMaxLevel_' + (currentChannelRowId)).val(), 10);
        CurrentChannelConfig.hv = parseInt($('#ServoHomeValue_' + (currentChannelRowId)).val(), 10);
        let ServoDataType = parseInt($('#ServoDataType_' + (currentChannelRowId)).val(), 10);

        CurrentChannelConfig.rev = (ServoDataType & 0x01) ? true : false;
        CurrentChannelConfig.sca = (ServoDataType & 0x02) ? true : false;
        CurrentChannelConfig.b16 = (ServoDataType & 0x04) ? true : false;
    });

} // ExtractPCA9685ConfigFromHtmlPage

function ExtractFishTankConfigFromHtmlPage(ChannelConfig) {

    // set the current selector value
    ChannelConfig.mode = parseInt($("#ftm_mode").find(":selected")[0].getAttribute("name"), 10);

    ChannelConfig.timeToColor.forEach(function (listEntry) {
        listEntry.mode = parseInt($("#ftmHour_" + listEntry.id).find(":selected")[0].getAttribute("name"), 10);
    });

    ChannelConfig.modeTable.forEach(function (listEntry) {
        if ("Cycle" !== listEntry.name) {
            listEntry.red = $("#FtMode_red_" + listEntry.id).val();
            listEntry.green = $("#FtMode_green_" + listEntry.id).val();
            listEntry.blue = $("#FtMode_blue_" + listEntry.id).val();
        }
    });

} // ExtractFishTankConfigFromHtmlPage

function ExtractChannelConfigFromHtmlPage(JsonConfig, SectionName) {
    // for each option channel:
    jQuery.each(JsonConfig, function (DisplayedChannelId, CurrentChannelConfigurationData) {
        let elementids = [];
        let modeControlName = '#' + SectionName + 'mode' + DisplayedChannelId;
        elementids = $(modeControlName + ' *[id]').filter(":input").map(function () {
            return $(this).attr('id');
        }).get();

        let ChannelType = parseInt($("#" + SectionName + DisplayedChannelId + " option:selected").val(), 10);
        let ChannelConfig = CurrentChannelConfigurationData[ChannelType];

        // tell the ESP what type of channel it should be using
        CurrentChannelConfigurationData.type = ChannelType;

        if ((ChannelConfig.type === "Relay") && ($("#relaychannelconfigurationtable").length)) {
            ExtractRelayConfigFromHtmlPage(ChannelConfig, SectionName);
        }
        else if ((ChannelConfig.type === "Servo PCA9685") && ($("#servo_pca9685channelconfigurationtable").length)) {
            ExtractPCA9685ConfigFromHtmlPage(ChannelConfig, SectionName);
        }
        else if (ChannelConfig.type === "Fish Tank") {
            ExtractFishTankConfigFromHtmlPage(ChannelConfig);
        }

        elementids.forEach(function (elementid) {
            let SelectedElement = modeControlName + ' #' + elementid;
            if ($(SelectedElement).is(':checkbox')) {
                ChannelConfig[elementid] = $(SelectedElement).prop('checked');
            }
            else {
                ChannelConfig[elementid] = $(SelectedElement).val();
            }
        });
    }); // end for each channel

} // ExtractChannelConfigFromHtmlPage

function ValidateConfigFields(ElementList) {
    // return true if errors were found
    let response = false;

    for (let ChildElementId = 0;
        ChildElementId < ElementList.length;
        ChildElementId++) {
        let ChildElement = ElementList[ChildElementId];
        // let ChildType = ChildElement.type;

        if ((ChildElement.validity.valid !== undefined) && (!$(ChildElement).hasClass('hidden'))) {
            // console.info("ChildElement.validity.valid: " + ChildElement.validity.valid);
            if (false === ChildElement.validity.valid) {
                // console.info("          Element: " + ChildElement.id);
                // console.info("   ChildElementId: " + ChildElementId);
                // console.info("ChildElement Type: " + ChildType);
                response = true;
            }
        }
    }
    return response;

} // ValidateConfigFields

// Build dynamic JSON config submission for "Device" tab
function submitDeviceConfig() {
    ExtractChannelConfigFromHtmlPage(Input_Config.channels, "input");
    Input_Config.ecb.enabled = $("#ecb_enable").is(':checked');
    Input_Config.ecb.id = $("#ecb_gpioid").val();
    Input_Config.ecb.polarity = $("#ecb_polarity").val();

    ExtractChannelConfigFromHtmlPage(Output_Config.channels, "output");

    submitNetworkConfig();
    wsEnqueue(JSON.stringify({ 'cmd': { 'set': { 'input': { 'input_config': Input_Config } } } }));
    wsEnqueue(JSON.stringify({ 'cmd': { 'set': { 'output': { 'output_config': Output_Config } } } }));

} // submitDeviceConfig

function convertUTCDateToLocalDate(date) {
    date = new Date(date);
    let localOffset = date.getTimezoneOffset() * 60000;
    let localTime = date.getTime();
    date = localTime - localOffset;

    return date;
} // convertUTCDateToLocalDate

function int2ip(num) {
    let d = num % 256;
    for (let i = 3; i > 0; i--) {
        num = Math.floor(num / 256);
        d = d + '.' + num % 256;
    }
    return d;
}

////////////////////////////////////////////////////
//
//  Websocket stuff
//
////////////////////////////////////////////////////
// On websocket connect
function wsConnect() {
    if ('WebSocket' in window) {
        if (!(target = ParseParameter('target'))) {
            target = document.location.host;
        }

        // target = "192.168.10.184";
        // target = "192.168.10.166";

        // Open a new web socket and set the binary type
        ws = new WebSocket('ws://' + target + '/ws');
        ws.binaryType = 'arraybuffer';
        ws.onclose = function (event) {
            console.error('WebSocket Close: ', event);
        };
        // When connection is opened, get core data.
        // Module data is loaded in module change / load callbacks
        ws.onopen = function (event) {
            console.info("ws.onopen " + event);

            // Start ping-pong heartbeat
            wsPingPong();

            $('#wserror').modal('hide');                               // Remove error modal
            $('.wsopt').empty();                                       // Clear out option data built from websockets

            // throw away any old messages
            // console.info("ws.onopen: Flush and Halt");
            wsFlushAndHaltTheOutputQueue();

            // show we are ready to start processing the output queue
            // console.info("ws.onopen: Turn On Sending");
            wsReadyToSend();

            // console.info("ws.onopen: Start Sending");
            // Push time
            wsEnqueue(JSON.stringify({ 'cmd': { 'set': { 'time': { 'time_t': convertUTCDateToLocalDate(Date()) / 1000 } } } }));

            // Process an admin message to populate AdminInfo
            wsEnqueue('XA');

            ProcessWindowChange($(location).attr("hash"));

            RequestStatusUpdate();  // start self filling status loop
        };

        ws.onmessage = function (event) {
            // reset the heartbeat timers
            wsPingPong();

            // console.info("ws.onmessage: Start");
            if (typeof event.data === "string") {
                console.debug("WS RECV: " + event.data);

                // Process "simple" X message format
                // Valid "Simple" message types
                //   GET_STATUS      = 'J',
                //   GET_ADMIN       = 'A',
                //   DO_RESET        = '6',
                //   DO_FACTORYRESET = '7',
                //   PING            = 'P',

                if (event.data.startsWith("X")) {
                    switch (event.data[1]) {
                        case 'J': {
                            let data = event.data.substr(2);
                            ProcessReceivedJsonStatusMessage(data);
                            break;
                        }
                        case 'P': {
                            // processed above for every received msg
                            // wsPingPong();
                            break;
                        }
                        case 'A': {
                            let data = event.data.substr(2);
                            ProcessReceivedJsonAdminMessage(data);
                            break;
                        }
                    }
                }
                else {
                    // console.info("ws.onmessage: Received: " + event.data);
                    let msg = JSON.parse(event.data);
                    // "GET" message is a response to a get request. Populate the frontend.
                    if ({}.hasOwnProperty.call(msg, "get")) {
                        ProcessReceivedJsonConfigMessage(msg.get);
                    }

                    //TODO: This never gets called now as we're sending 'cmd': 'OK' back instead of 'set' with the updated config
                    // "SET" message is a response to a set request. Data has been validated and saved, Populate the frontend.
                    if ({}.hasOwnProperty.call(msg, "set")) {
                        ProcessReceivedJsonConfigMessage(msg.set);
                        snackSave();
                    }

                    //TODO: Inform user configuration was saved, but this is broken as the UI could be in an invalid state
                    //      if the validation routines changed their config. To be fixed in UI update.
                    if ({}.hasOwnProperty.call(msg, 'cmd')) {
                        if (msg.cmd === 'OK') {
                            // console.log('---- OK ----');
                            snackSave();
                        }
                    }
                }
            }
            else {
                // console.info("Stream Data");

                let streamData = new Uint8Array(event.data);
                drawStream(streamData);
                if ($('#diag').is(':visible')) {
                    wsEnqueue('V1');
                }
            }

            // show we are ready to send
            // console.info("ws.onmessage: Ask To Send Next Msg");
            wsReadyToSend();

            // console.info("ws.onmessage: Done");
        }; // onmessage

        ws.onerror = function (event) {
            console.error("WebSocket error: ", event);

        };
    }
    else {
        alert('WebSockets is NOT supported by your Browser! You will need to upgrade your browser or downgrade to v2.0 of the ESPixelStick firmware.');
    }
}

// Ping every 4sec, Reconnect after 12sec
function wsPingPong() {
    // Ping Pong connection detection
    clearTimeout(pingTimer);
    clearTimeout(pongTimer);

    if (false === IsDocumentHidden) {
        pingTimer = setTimeout(function () {
            // is the socket still open?
            if (ws.readyState === 3) {
                wsReconnect();
            }
            else {
                ws.send('XP');
                // wsEnqueue('XP');
            }

        }, 1000);

        pongTimer = setTimeout(function () {
            wsReconnect();
        }, 6000);
    }
}

// Attempt to reconnect
function wsReconnect() {
    $('#wserror').modal();
    clearTimeout(pingTimer);
    clearTimeout(pongTimer);
    wsFlushAndHaltTheOutputQueue();
    ws.close();
    ws = null;
    wsConnect();
}

// Websocket message queuer
function wsEnqueue(message) {
    // only send messages if the WS interface is up and document is visible
    if (ws.readyState !== 1) {
        console.debug("WS is down - readyState: " + ws.readyState);
        console.debug("WS is down - Discarding msg: " + message);
    }

    else if (wsPaused) {
        console.debug("WS Paused - Discarding msg: " + message)
    }

    else {
        wsOutputQueue.push(message);
        wsProcessOutputQueue();

    } // WS is up
} // wsEnqueue

function wsFlushAndHaltTheOutputQueue() {
    // do we have a send timer running?
    if (null !== wsOutputQueueTimer) {
        // stop the timer
        clearTimeout(wsOutputQueueTimer);
        wsOutputQueueTimer = null;
    }

    // show we are ready NOT to send the next message
    wsBusy = true;

    // empty the output queue
    while (wsOutputQueue.length > 0) {
        //get the next message from the queue.
        let message = wsOutputQueue.shift();
        console.debug("Discarding msg: " + message);
    }
} // wsFlushAndHaltTheOutputQueue

// Websocket message queuer
function wsProcessOutputQueue() {
    // console.log('wsProcessOutputQueue');

    // only send messages if the WS interface is up
    if (ws.readyState !== 1) {
        // The interface is NOT up. Flush the queue
        // console.log('wsProcessOutputQueue: WS Down. Flush');
        wsFlushAndHaltTheOutputQueue();
    }

    // Pause processing
    else if (document.hidden) {
        console.debug(`WS Paused - Holding msg: ${wsOutputQueue}`);
        wsPaused = true;
        if (null !== wsOutputQueueTimer) {
            // stop the timer
            clearTimeout(wsOutputQueueTimer);
            wsOutputQueueTimer = null;
            wsBusy = true;
        }
    }

    //check if we are currently waiting for a response
    else if (wsBusy === true) {
        // console.log('wsProcessOutputQueue: Busy');
    } // cant send yet

    else if (wsOutputQueue.length > 0) {
        //set the wsBusy flag indicating that we are waiting for a response
        wsBusy = true;

        //get the next message from the queue.
        let OutputMessage = wsOutputQueue.shift();

        // set WaitForResponseTimeMS to clear flag and try next message if response
        // isn't received.
        let WaitForResponseTimeMS = 5000; // 5 seconds

        // Short WaitForResponseTimeMS for message types that don't generate a response.
        let UseShortDelay = ['T0', 'T1', 'T2', 'T3', 'T4', 'T5', 'T6', 'T7', 'X6'].indexOf(OutputMessage.substr(0, 2));
        if (UseShortDelay !== -1) {
            // warning, setting this value too low can cause a rentrance issue
            WaitForResponseTimeMS = 50;
        }

        // set up a new timer
        wsOutputQueueTimer = setTimeout(function () {
            // console.info('WS Send Timer expired');

            // Move on to the next message
            wsReadyToSend();

        }, WaitForResponseTimeMS);

        //send it.
        console.debug('WS SEND: ' + OutputMessage);
        ws.send(OutputMessage);

    } // message available to send

} // wsProcessOutputQueue

// Websocket message queuer
function wsReadyToSend() {
    // is a timer running?
    if (null !== wsOutputQueueTimer) {
        // stop the timer
        clearTimeout(wsOutputQueueTimer);
        wsOutputQueueTimer = null;
    }

    // show we are ready to send the next message
    wsBusy = false;
    wsPaused = false;

    //send next message
    wsProcessOutputQueue();

} // wsReadyToSend

// Move to diagnostics
function drawStream(streamData) {
    let cols = parseInt($('#v_columns').val());
    let size = Math.floor((canvas.width - 20) / cols);
    let maxDisplay = 0;

    if ($("#diag #viewStyle option:selected").val() === "rgb") {
        maxDisplay = Math.min(streamData.length, (cols * Math.floor((canvas.height - 30) / size)) * 3);
        for (let i = 0; i < maxDisplay; i += 3) {
            ctx.fillStyle = 'rgb(' + streamData[i + 0] + ',' + streamData[i + 1] + ',' + streamData[i + 2] + ')';
            let col = (i / 3) % cols;
            let row = Math.floor((i / 3) / cols);
            ctx.fillRect(10 + (col * size), 10 + (row * size), size - 1, size - 1);
        }
    }
    else if ($("#diag #viewStyle option:selected").val() === "rgbw") {
        maxDisplay = Math.min(streamData.length, (cols * Math.floor((canvas.height - 30) / size)) * 4);
        for (let i = 0; i < maxDisplay; i += 4) {
            let WhiteLevel = streamData[i + 3];
            ctx.fillStyle = 'rgb(' + Math.max(streamData[i + 0], WhiteLevel) + ',' + Math.max(streamData[i + 1], WhiteLevel) + ',' + Math.max(streamData[i + 2], WhiteLevel) + ')';
            let col = (i / 4) % cols;
            let row = Math.floor((i / 4) / cols);
            ctx.fillRect(10 + (col * size), 10 + (row * size), size - 1, size - 1);
        }
    }
    else {
        maxDisplay = Math.min(streamData.length, (cols * Math.floor((canvas.height - 30) / size)));
        for (let i = 0; i < maxDisplay; i++) {
            ctx.fillStyle = 'rgb(' + streamData[i] + ',' + streamData[i] + ',' + streamData[i] + ')';
            let col = (i) % cols;
            let row = Math.floor(i / cols);
            ctx.fillRect(10 + (col * size), 10 + (row * size), size - 2, size - 2);
        }
    }
    if (streamData.length > maxDisplay) {
        ctx.fillStyle = 'rgb(204,0,0)';
        ctx.fillRect(0, canvas.height - 25, canvas.width, 25);
        ctx.fillStyle = 'rgb(255,255,255)';
        ctx.fillText("Increase number of columns to show all data", (canvas.width / 2), canvas.height - 5);
    }
}

// Move to diagnostics
function clearStream() {
    if (typeof ctx !== 'undefined') {
        ctx.clearRect(0, 0, canvas.width, canvas.height);
    }
}

function ProcessReceivedJsonAdminMessage(data) {
    let ParsedJsonAdmin = JSON.parse(data);
    AdminInfo = ParsedJsonAdmin.admin;

    $('#version').text(AdminInfo.version);
    $('#built').text(AdminInfo.built);
    $('#arch').text(AdminInfo.arch);
    $('#usedflashsize').text(AdminInfo.usedflashsize);
    $('#realflashsize').text(AdminInfo.realflashsize);
    $('#flashchipid').text(AdminInfo.flashchipid);

    // Hide elements that are not applicable to our architecture
    if (AdminInfo.arch === "ESP8266") {
        $('.esp32').addClass('hidden');
    } else if (AdminInfo.arch === "ESP32") {
        $('.esp8266').addClass('hidden');
    }

} // ProcessReceivedJsonAdminMessage

// ProcessReceivedJsonStatusMessage
function ProcessReceivedJsonStatusMessage(data) {
    let JsonStat = JSON.parse(data);
    let Status = JsonStat.status;
    let System = Status.system;
    let Network = System.network;
    let Wifi = Network.wifi;
    let Ethernet = Network.wifi;

    let rssi = Wifi.rssi;
    let quality = 2 * (rssi + 100);

    if (rssi <= -100) {
        quality = 0;
    }
    else if (rssi >= -50) {
        quality = 100;
    }

    $('#w_connected').text((true === Wifi.connected) ? "Yes" : "No");
    $('#w_hostname').text(Wifi.hostname);
    $('#w_rssi').text(rssi);
    $('#w_quality').text(quality);
    $('#w_ssid').text(Wifi.ssid);
    $('#w_ip').text(Wifi.ip);
    $('#w_subnet').text(Wifi.subnet);
    $('#w_mac').text(Wifi.mac);

    if ({}.hasOwnProperty.call(Network, 'eth')) {
        $('#ethernet_status').removeClass("hidden")
        Ethernet = Network.eth;
        $('#e_connected').text((true === Ethernet.connected) ? "Yes" : "No");
        $('#e_hostname').text(Ethernet.hostname);
        $('#e_ip').text(Ethernet.ip);
        $('#e_subnet').text(Ethernet.subnet);
        $('#e_mac').text(Ethernet.mac);
    }
    else {
        $('#ethernet_status').addClass("hidden")
    }

    // getHeap(data)
    $('#x_freeheap').text(System.freeheap);

    // getUptime
    // uptime is reported in milliseconds
    let date = new Date(System.uptime);
    let str = '';

    //    let hoursPerDay = 24;
    //    let MinutesPerHour = 60;
    //    let secondsPerMinute = 60;
    //    let millisecondsPerSecond = 1000;
    //    let MillisecondsPerDay = millisecondsPerSecond * secondsPerMinute * MinutesPerHour * hoursPerDay; = 86400000

    str += Math.floor(date.getTime() / 86400000) + " days, ";
    str += ("0" + date.getUTCHours()).slice(-2) + ":";
    str += ("0" + date.getUTCMinutes()).slice(-2) + ":";
    str += ("0" + date.getUTCSeconds()).slice(-2);
    $('#x_uptime').text(str);

    if ({}.hasOwnProperty.call(System, 'used')) {
        $('#i_size').removeClass("hidden");
        $('#x_size').removeClass("hidden");
        $('#x_size').text(System.size);

        $('#i_used').removeClass("hidden");
        $('#x_used').removeClass("hidden");
        $('#x_used').text(System.used);
    }
    else {
        $('#i_size').addClass("hidden");
        $('#x_size').addClass("hidden");
        $('#i_used').addClass("hidden");
        $('#x_used').addClass("hidden");
    }

    if (true === System.SDinstalled) {
        $("#li-filemanagement").removeClass("hidden");
    }
    else {
        $("#li-filemanagement").addClass("hidden");
    }

    // getE131Status(data)
    let InputStatus = Status.input[0];
    if ({}.hasOwnProperty.call(InputStatus, 'e131')) {
        $('#E131Status').removeClass("hidden")

        $('#uni_first').text(InputStatus.e131.unifirst);
        $('#uni_last').text(InputStatus.e131.unilast);
        $('#pkts').text(InputStatus.e131.num_packets);
        $('#chanlim').text(InputStatus.e131.unichanlim);
        $('#perr').text(InputStatus.e131.packet_errors);
        $('#clientip').text(int2ip(parseInt(InputStatus.e131.last_clientIP)));
    }
    else {
        $('#E131Status').addClass("hidden")
    }

    if ({}.hasOwnProperty.call(InputStatus, 'Artnet')) {
        $('#ArtnetStatus').removeClass("hidden")

        $('#an_uni_first').text(InputStatus.Artnet.unifirst);
        $('#an_uni_last').text(InputStatus.Artnet.unilast);
        $('#an_pkts').text(InputStatus.Artnet.num_packets);
        $('#an_chanlim').text(InputStatus.Artnet.unichanlim);
        $('#an_perr').text(InputStatus.Artnet.packet_errors);
        $('#an_clientip').text(InputStatus.Artnet.last_clientIP);
    }
    else {
        $('#ArtnetStatus').addClass("hidden")
    }

    if ({}.hasOwnProperty.call(InputStatus, 'ddp')) {
        $('#ddpStatus').removeClass("hidden")

        $('#ddppacketsreceived').text(InputStatus.ddp.packetsreceived);
        $('#ddpbytesreceived').text(InputStatus.ddp.bytesreceived);
        $('#ddperrors').text(InputStatus.ddp.errors);
    }
    else {
        $('#ddpStatus').addClass("hidden")
    }

    InputStatus = Status.input[1];

    if ({}.hasOwnProperty.call(InputStatus, 'Player')) {
        let PlayerStatus = InputStatus.Player;
        if ({}.hasOwnProperty.call(PlayerStatus, 'FPPDiscovery')) {
            $('#FPPRemoteStatus').removeClass("hidden")

            let FPPDstatus = PlayerStatus.FPPDiscovery

            $('#fppsyncreceived').text(FPPDstatus.SyncCount);
            $('#fppsyncadjustments').text(FPPDstatus.SyncAdjustmentCount);
            $('#fppremoteip').text(FPPDstatus.FppRemoteIp);
            $('#fppplayedfilecount').text(FPPDstatus.PlayedFileCount);

            $('#fppremoteFilePlayerFilename').text(FPPDstatus.current_sequence);
            $('#fppremoteFilePlayerTimeElapsed').text(FPPDstatus.time_elapsed);
            $('#fppremoteFilePlayerTimeRemaining').text(FPPDstatus.time_remaining);
            $('#fppremotelasterror').text(FPPDstatus.errors);
        }
        else {
            $('#FPPRemoteStatus').addClass("hidden")
        }

        if ({}.hasOwnProperty.call(PlayerStatus, 'File')) {
            $('#LocalFilePlayerStatus').removeClass("hidden");

            let FilePlayerStatus = PlayerStatus.File;
            $('#localFilePlayerFilename').text(FilePlayerStatus.current_sequence);
            $('#localFilePlayerTimeElapsed').text(FilePlayerStatus.time_elapsed);
            $('#localFilePlayerTimeRemaining').text(FilePlayerStatus.time_remaining);
        }
        else {
            $('#LocalFilePlayerStatus').addClass("hidden");
        }

        if ({}.hasOwnProperty.call(PlayerStatus, 'Effect')) {
            $('#LocalEffectPlayerStatus').removeClass("hidden");

            $('#localFilePlayerEffectName').text(PlayerStatus.Effect.currenteffect);
            $('#localFilePlayerEffectTimeRemaining').text(PlayerStatus.Effect.TimeRemaining);
        }
        else {
            $('#LocalEffectPlayerStatus').addClass("hidden")
        }

        if ({}.hasOwnProperty.call(PlayerStatus, 'Paused')) {
            $('#PausedPlayerStatus').removeClass("hidden");

            $('#PausedTimeRemaining').text(PlayerStatus.Paused.TimeRemaining);
        }
        else {
            $('#PausedPlayerStatus').addClass("hidden")
        }
    }
    else {
        $('#LocalPlayListPlayerStatus').addClass("hidden");
        $('#LocalFilePlayerStatus').addClass("hidden");
        $('#LocalEffectPlayerStatus').addClass("hidden");
        $('#PausedPlayerStatus').addClass("hidden");
        $('#FPPRemoteStatus').addClass("hidden");
    }

    // Device Refresh is dynamic
    // #refresh is used in device config tab to reflect what refresh rate should be, not what it currently is
    // $('#refresh').text(Status.output[0].framerefreshrate + " fps");
} // ProcessReceivedJsonStatusMessage

// Show "save" snackbar for 3sec
function snackSave() {
    let x = document.getElementById('snackbar');
    x.className = 'show';
    setTimeout(function () {
        x.className = x.className.replace('show', '');
    }, 3000);
}

// Show reboot modal
function showReboot() {
    $("#EfuProgressBar").addClass("hidden");
    $('#update').modal('hide');
    $('#reboot').modal();
    setTimeout(function () {
        if ($('#wifi #dhcp').prop('checked')) {
            window.location.assign("/");
        }
        else {
            window.location.assign("http://" + $('#ip').val());
        }
    }, 5000);
}

// Queue reboot
function reboot() {
    showReboot();
    wsEnqueue('X6');
}

// Reset config
$('#confirm-reset .btn-ok').click(function () {
    showReboot();
    wsEnqueue('X7');
});
