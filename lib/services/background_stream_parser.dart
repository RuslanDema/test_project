import 'dart:convert';

import '../models/telemetry_model.dart';

class ParserMetrics {
  int corruptedPacketsCounter = 0;
}

class BackgroundStreamParser {
  final ParserMetrics metrics = ParserMetrics();

  List<TelemetryPayload> parseLine(String line) {
    final List<TelemetryPayload> results = [];
    line = line.trim();
    if (line.isNotEmpty) {
      try {
        final Map<String, dynamic> decoded = jsonDecode(line);
        if (!decoded.containsKey('ack')) {
          results.add(TelemetryPayload.fromJson(decoded));
        }
      } catch (_) {
        metrics.corruptedPacketsCounter++;
      }
    }
    return results;
  }
}
