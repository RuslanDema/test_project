import 'dart:async';
import 'dart:convert';
import 'dart:typed_data';

import 'i_transport_service.dart';

class MockTelemetryService implements ITransportService {
  final _stateController = StreamController<DeviceConnectionState>.broadcast();
  final _rawStreamController = StreamController<Uint8List>.broadcast();
  Timer? _dataGeneratorTimer;
  int _seq = 0;

  @override
  Stream<DeviceConnectionState> get connectionState => _stateController.stream;

  @override
  DeviceConnectionState currentState = DeviceConnectionState.disconnected;

  @override
  Stream<Uint8List> get rawInputStream => _rawStreamController.stream;

  @override
  Future<void> connect(String deviceAddress) async {
    currentState = DeviceConnectionState.connecting;
    _stateController.add(currentState);

    await Future.delayed(const Duration(milliseconds: 500));

    currentState = DeviceConnectionState.connected;
    _stateController.add(currentState);

    _dataGeneratorTimer = Timer.periodic(const Duration(milliseconds: 100), (
      _,
    ) {
      _seq++;
      final mockJson = jsonEncode({
        "seq": _seq,
        "timestamp_ms": DateTime.now().millisecondsSinceEpoch,
        "ch1": {"ma": 12.00, "mm": 1.00, "err": 0.20, "st": "OK"},
        "ch2": {"ma": 4.01, "mm": 0.01, "err": 0.05, "st": "OK"},
        "ch3": {"ma": 16.05, "mm": 1.51, "err": 1.25, "st": "WARN"},
        "gen": {"mode": "ACT_24V", "target_ma": 12.00},
        "bat": {"volts": 3.82, "percent": 82},
      });

      _rawStreamController.add(Uint8List.fromList(utf8.encode('$mockJson\n')));
    });
  }

  @override
  Future<void> disconnect() async {
    _dataGeneratorTimer?.cancel();
    currentState = DeviceConnectionState.disconnected;
    _stateController.add(currentState);
  }

  @override
  Future<void> send(Uint8List bytes) async {
    final decoded = utf8.decode(bytes);
    if (decoded.contains('trans_id')) {
      final map = jsonDecode(decoded);
      final transId = map['trans_id'];

      Future.delayed(const Duration(milliseconds: 50), () {
        final ackJson = jsonEncode({"ack": transId, "status": "SUCCESS"});
        _rawStreamController.add(Uint8List.fromList(utf8.encode('$ackJson\n')));
      });
    }
  }

  @override
  void dispose() {
    _dataGeneratorTimer?.cancel();
    _stateController.close();
    _rawStreamController.close();
  }
}
