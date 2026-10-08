import 'dart:async';
import 'dart:convert';
import 'dart:typed_data';

import 'i_transport_service.dart';

class CommandResponse {
  final int transId;
  final bool isSuccess;
  final String statusText;

  CommandResponse({
    required this.transId,
    required this.isSuccess,
    required this.statusText,
  });
}

class CommandDispatcher {
  final ITransportService transport;
  int _transIdCounter = 1;

  Completer<CommandResponse>? _pendingCommandCompleter;
  int? _activeTransId;

  CommandDispatcher(this.transport);

  void handleIncomingRawJson(String jsonString) {
    try {
      final Map<String, dynamic> decoded = jsonDecode(jsonString);
      if (decoded.containsKey('ack')) {
        final ackId = (decoded['ack'] as num).toInt();
        final status = decoded['status'] as String? ?? 'FAILED';

        if (_activeTransId == ackId && _pendingCommandCompleter != null) {
          _pendingCommandCompleter!.complete(
            CommandResponse(
              transId: ackId,
              isSuccess: status == 'SUCCESS',
              statusText: status,
            ),
          );
          _activeTransId = null;
        }
      }
    } catch (_) {}
  }

  Future<CommandResponse> sendSetGeneratorCommand({
    required String mode,
    required double targetMa,
  }) async {
    if (transport.currentState != DeviceConnectionState.connected) {
      return CommandResponse(
        transId: -1,
        isSuccess: false,
        statusText: 'NOT_CONNECTED',
      );
    }

    if (_pendingCommandCompleter != null &&
        !_pendingCommandCompleter!.isCompleted) {
      return CommandResponse(
        transId: -1,
        isSuccess: false,
        statusText: 'DISPATCHER_BUSY',
      );
    }

    final currentTransId = _transIdCounter++;
    _activeTransId = currentTransId;
    _pendingCommandCompleter = Completer<CommandResponse>();

    final commandPayload = {
      "cmd": "SET_GENERATOR",
      "trans_id": currentTransId,
      "params": {"mode": mode, "target_ma": targetMa},
    };

    final rawBytes = Uint8List.fromList(
      utf8.encode('${jsonEncode(commandPayload)}\n'),
    );
    await transport.send(rawBytes);

    return _pendingCommandCompleter!.future.timeout(
      const Duration(milliseconds: 1500),
      onTimeout: () {
        _activeTransId = null;
        _pendingCommandCompleter = null;
        return CommandResponse(
          transId: currentTransId,
          isSuccess: false,
          statusText: 'TIMEOUT_ERROR',
        );
      },
    );
  }
}
