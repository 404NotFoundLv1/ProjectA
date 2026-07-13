#include "Diagnostics/PRDiagnosticsTypes.h"

bool FPRDiagnosticEvent::IsValid() const
{
	return Sequence > 0 && TimestampUtc.GetTicks() > 0 && !Category.IsNone() && !EventCode.IsNone() && !Message.IsEmpty();
}

bool FPRDiagnosticFilter::Matches(const FPRDiagnosticEvent& Event) const
{
	if (static_cast<uint8>(Event.Severity) < static_cast<uint8>(MinimumSeverity))
	{
		return false;
	}
	if (!Categories.IsEmpty() && !Categories.Contains(Event.Category))
	{
		return false;
	}
	if (SearchText.IsEmpty())
	{
		return true;
	}
	return Event.Message.Contains(SearchText, ESearchCase::IgnoreCase)
		|| Event.EventCode.ToString().Contains(SearchText, ESearchCase::IgnoreCase)
		|| Event.Category.ToString().Contains(SearchText, ESearchCase::IgnoreCase);
}

