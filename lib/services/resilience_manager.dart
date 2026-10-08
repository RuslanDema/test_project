import 'dart:async';

import 'i_transport_service.dart';

class ResilienceManager {
  final ITransportService transport;
  Timer? _heartbeatTimer;
  bool _isReconnecting = false;
  int _reconnectAttempts = 0;
  static const int maxAttempts = 10;

  final StreamController<bool> _reconnectStatusController =
      StreamController.broadcast();
  Stream<bool> get onReconnectStatusChanged =>
      _reconnectStatusController.stream;

  ResilienceManager(this.transport) {
    transport.connectionState.listen((state) {
      if (state == DeviceConnectionState.connected) {
        _resetHeartbeat();
        _reconnectAttempts = 0;
        _isReconnecting = false;
      } else if (state == DeviceConnectionState.connectionLost) {
        _triggerAutoReconnect();
      }
    });
  }

  void notifyPacketReceived() {
    _resetHeartbeat();
  }

  void _resetHeartbeat() {
    _heartbeatTimer?.cancel();
    _heartbeatTimer = Timer(const Duration(milliseconds: 2500), () {
      if (transport.currentState == DeviceConnectionState.connected) {
        _triggerAutoReconnect();
      }
    });
  }

  Future<void> _triggerAutoReconnect() async {
    if (_isReconnecting) return;
    _isReconnecting = true;
    _heartbeatTimer?.cancel();

    while (_reconnectAttempts < maxAttempts && _isReconnecting) {
      _reconnectAttempts++;
      _reconnectStatusController.add(true);

      int delaySeconds = 5;
      if (_reconnectAttempts == 1) delaySeconds = 1;
      if (_reconnectAttempts == 2) delaySeconds = 3;

      await Future.delayed(Duration(seconds: delaySeconds));

      try {
        await transport.connect("PN2026_LAST_DEVICE");
        if (transport.currentState == DeviceConnectionState.connected) {
          _isReconnecting = false;
          _reconnectStatusController.add(false);
          return;
        }
      } catch (_) {}
    }

    _isReconnecting = false;
    _reconnectStatusController.add(false);
  }

  void dispose() {
    _heartbeatTimer?.cancel();
    _reconnectStatusController.close();
  }
}
