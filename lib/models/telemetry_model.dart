class ChannelData {
  final double ma;
  final double mm;

  ChannelData({
    required this.ma,
    required this.mm,
  });

  factory ChannelData.fromJson(Map<String, dynamic> json) {
    return ChannelData(
      ma: (json['ma'] as num?)?.toDouble() ?? 0.0,
      mm: (json['mm'] as num?)?.toDouble() ?? 0.0,
    );
  }
}

class TelemetryModel {
  final ChannelData ch1;
  final ChannelData ch2;
  final ChannelData ch3;
  final double batteryPercent;
  final double batteryVolts;

  TelemetryModel({
    required this.ch1,
    required this.ch2,
    required this.ch3,
    required this.batteryPercent,
    required this.batteryVolts,
  });

  factory TelemetryModel.fromJson(Map<String, dynamic> json) {
    return TelemetryModel(
      ch1: json['ch1'] != null
          ? ChannelData.fromJson(json['ch1'])
          : ChannelData(ma: 0, mm: 0),
      ch2: json['ch2'] != null
          ? ChannelData.fromJson(json['ch2'])
          : ChannelData(ma: 0, mm: 0),
      ch3: json['ch3'] != null
          ? ChannelData.fromJson(json['ch3'])
          : ChannelData(ma: 0, mm: 0),
      batteryPercent: (json['bat']?['percent'] as num?)?.toDouble() ?? 100.0,
      batteryVolts: (json['bat']?['volts'] as num?)?.toDouble() ?? 12.0,
    );
  }
}

// Псевдоним для совместимости со старыми сервисами
typedef TelemetryPayload = TelemetryModel;
