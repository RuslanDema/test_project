import '../models/telemetry_model.dart';
import '../services/i_transport_service.dart';

class DeviceState {
  final DeviceConnectionState connectionState;
  final TelemetryModel? telemetry;
  final String statusMessage;
  final bool isSendingCommand;

  const DeviceState({
    this.connectionState = DeviceConnectionState.disconnected,
    this.telemetry,
    this.statusMessage = 'Отключено',
    this.isSendingCommand = false,
  });

  DeviceState copyWith({
    DeviceConnectionState? connectionState,
    TelemetryModel? telemetry,
    String? statusMessage,
    bool? isSendingCommand,
  }) {
    return DeviceState(
      connectionState: connectionState ?? this.connectionState,
      telemetry: telemetry ?? this.telemetry,
      statusMessage: statusMessage ?? this.statusMessage,
      isSendingCommand: isSendingCommand ?? this.isSendingCommand,
    );
  }
}
